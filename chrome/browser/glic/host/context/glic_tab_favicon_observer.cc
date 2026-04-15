// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_tab_favicon_observer.h"

#include <memory>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/common/future_browser_features.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/chrome_features.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/tab_favicon.h"
#endif

namespace glic {
namespace {

#if BUILDFLAG(IS_ANDROID)
constexpr float kFaviconScale = 1.0f;
#else
constexpr float kFaviconScale = 2.0f;
#endif

class FaviconData {
 public:
  FaviconData() = default;
  FaviconData(const FaviconData&) = default;
  FaviconData& operator=(const FaviconData&) = default;

  static FaviconData FromImage(gfx::Image image) { return FaviconData(image); }

  static FaviconData FromWebContents(content::WebContents& web_contents) {
#if BUILDFLAG(IS_ANDROID)
    // ContentFaviconDriver::GetFavicon() doesn't work on Android.
    TabAndroid* tab_android = TabAndroid::FromWebContents(&web_contents);
    if (!tab_android) {
      return FaviconData();
    }
    return FaviconData(TabFavicon::GetBitmapForTab(tab_android));
#else
    favicon::ContentFaviconDriver* favicon_driver =
        favicon::ContentFaviconDriver::FromWebContents(&web_contents);
    if (!favicon_driver || !favicon_driver->FaviconIsValid()) {
      return FaviconData();
    }
    return FaviconData(favicon_driver->GetFavicon());
#endif
  }

  const SkBitmap& GetBitmap() const {
    if (bitmap_.has_value()) {
      return *bitmap_;
    }
    if (image_.IsEmpty()) {
      bitmap_ = SkBitmap();
    } else {
      bitmap_ =
          image_.ToImageSkia()->GetRepresentation(kFaviconScale).GetBitmap();
    }
    return *bitmap_;
  }

  bool operator==(const FaviconData& other) const {
    // Ignore image if empty, they're not always available.
    if (!image_.IsEmpty() && image_ == other.image_) {
      return true;
    }
    return FaviconEquals(GetBitmap(), other.GetBitmap());
  }

 private:
  explicit FaviconData(gfx::Image image) : image_(image) {}
  explicit FaviconData(SkBitmap bitmap) : bitmap_(bitmap) {}

  // The actual bitmap to notify receivers of. May be lazily computed.
  mutable std::optional<SkBitmap> bitmap_;

  // The source image, if it can be found. Used only to detect changes more
  // efficiently.
  gfx::Image image_;
};

}  // namespace

// Holds receivers and FaviconData, and notifies receivers when the favicon
// changes.
class FaviconNotifier {
 public:
  void SetFaviconAndNotify(const FaviconData& favicon_data) {
    if (favicon_data == favicon_data_) {
      return;
    }
    favicon_data_ = favicon_data;
    NotifyFaviconChanged();
  }

  void Subscribe(::mojo::PendingRemote<mojom::TabFaviconHandler> receiver) {
    mojo::Remote<mojom::TabFaviconHandler> new_remote;
    new_remote.Bind(std::move(receiver));
    new_remote->OnTabFaviconChanged(favicon_data_.GetBitmap());
    receivers_.Add(std::move(new_remote));
  }

  mojo::RemoteSet<mojom::TabFaviconHandler>& receivers() { return receivers_; }
  const mojo::RemoteSet<mojom::TabFaviconHandler>& receivers() const {
    return receivers_;
  }

  const FaviconData& favicon_data() const { return favicon_data_; }

 private:
  void NotifyFaviconChanged() {
    for (auto& receiver : receivers_) {
      receiver->OnTabFaviconChanged(favicon_data_.GetBitmap());
    }
  }

  mojo::RemoteSet<mojom::TabFaviconHandler> receivers_;
  FaviconData favicon_data_;
};

class GlicTabFaviconObserver::TabObserver
    : public content::WebContentsObserver,
      public favicon::FaviconDriverObserver {
 public:
  TabObserver(GlicTabFaviconObserver* owner_observer, tabs::TabInterface* tab)
      : content::WebContentsObserver(tab->GetContents()),
        owner_observer_(owner_observer),
        tab_(tab) {
    notifier_.receivers().set_disconnect_handler(base::BindRepeating(
        &TabObserver::OnDisconnected, base::Unretained(this)));
    will_detach_subscription_ = tab_->RegisterWillDetach(base::BindRepeating(
        &TabObserver::OnWillDetach, base::Unretained(this)));
    will_discard_contents_subscription_ =
        tab_->RegisterWillDiscardContents(base::BindRepeating(
            &TabObserver::OnWillDiscardContents, base::Unretained(this)));

    if (web_contents()) {
      auto* favicon_driver =
          favicon::ContentFaviconDriver::FromWebContents(web_contents());
      if (favicon_driver) {
        favicon_driver->AddObserver(this);
      }
      notifier_.SetFaviconAndNotify(
          FaviconData::FromWebContents(*web_contents()));
    }
  }

  ~TabObserver() override {
    if (web_contents()) {
      auto* favicon_driver =
          favicon::ContentFaviconDriver::FromWebContents(web_contents());
      if (favicon_driver) {
        favicon_driver->RemoveObserver(this);
      }
    }
  }

  void Subscribe(::mojo::PendingRemote<mojom::TabFaviconHandler> receiver) {
    notifier_.Subscribe(std::move(receiver));
  }

  bool HasReceivers() const { return !notifier_.receivers().empty(); }

 private:
  void OnDisconnected(mojo::RemoteSetElementId element_id) {
    if (notifier_.receivers().empty()) {
      owner_observer_->ScheduleCleanupForTab(tab_->GetHandle());
    }
  }

  void OnWillDetach(tabs::TabInterface* tab,
                    tabs::TabInterface::DetachReason reason) {
    if (reason == tabs::TabInterface::DetachReason::kDelete) {
      // Destroys `this`.
      owner_observer_->OnTabWillClose(tab->GetHandle());
    }
  }

  void OnWillDiscardContents(tabs::TabInterface* tab,
                             content::WebContents* previous_contents,
                             content::WebContents* new_contents) {
    if (web_contents()) {
      auto* old_driver =
          favicon::ContentFaviconDriver::FromWebContents(web_contents());
      if (old_driver) {
        old_driver->RemoveObserver(this);
      }
    }
    Observe(new_contents);
    if (new_contents) {
      auto* new_driver =
          favicon::ContentFaviconDriver::FromWebContents(new_contents);
      if (new_driver) {
        new_driver->AddObserver(this);
      }
    }
  }

  // favicon::FaviconDriverObserver:
  void OnFaviconUpdated(favicon::FaviconDriver* favicon_driver,
                        NotificationIconType notification_icon_type,
                        const GURL& icon_url,
                        bool icon_url_changed,
                        const gfx::Image& image) override {
    // On Android, the favicon driver will fetch larger favicons. We filter here
    // to ignore unexpected favicon types.
#if BUILDFLAG(IS_ANDROID)
    if (notification_icon_type != FaviconDriverObserver::NON_TOUCH_LARGEST &&
        notification_icon_type != FaviconDriverObserver::TOUCH_LARGEST) {
      return;
    }
#else
    if (notification_icon_type != FaviconDriverObserver::NON_TOUCH_16_DIP) {
      return;
    }
#endif

    notifier_.SetFaviconAndNotify(FaviconData::FromImage(image));
  }

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override {
    if (!web_contents()) {
      return;
    }
    notifier_.SetFaviconAndNotify(
        FaviconData::FromWebContents(*web_contents()));
  }

  // Owns this.
  raw_ptr<GlicTabFaviconObserver> owner_observer_;
  raw_ptr<tabs::TabInterface> tab_;
  FaviconNotifier notifier_;

  base::CallbackListSubscription will_detach_subscription_;
  base::CallbackListSubscription will_discard_contents_subscription_;
};

GlicTabFaviconObserver::GlicTabFaviconObserver(Profile* profile)
    : profile_(profile) {}
GlicTabFaviconObserver::~GlicTabFaviconObserver() = default;

void GlicTabFaviconObserver::OnTabWillClose(tabs::TabHandle tab_handle) {
  observers_.erase(tab_handle);
}

void GlicTabFaviconObserver::SubscribeToTabFavicon(
    int32_t tab_id,
    mojo::PendingRemote<mojom::TabFaviconHandler> remote) {
  tabs::TabInterface::Handle handle(tab_id);
  tabs::TabInterface* tab = handle.Get();
  if (!tab) {
    remote.reset();
    return;
  }
  if (tab->GetBrowserWindowInterface()->GetProfile() != profile_) {
    remote.reset();
    return;
  }
  TabObserver* observer_ptr = nullptr;
  auto iter = observers_.find(handle);
  if (iter != observers_.end()) {
    observer_ptr = iter->second.get();
  } else {
    auto observer = std::make_unique<TabObserver>(this, tab);
    observer_ptr = observer.get();
    observers_.insert({handle, std::move(observer)});
  }
  observer_ptr->Subscribe(std::move(remote));
}

// Schedules deleting the tab observer later. This isn't done immediately
// to avoid teardown if the observer is used again quickly.
void GlicTabFaviconObserver::ScheduleCleanupForTab(tabs::TabHandle tab_handle) {
  pending_cleanup_.insert(tab_handle);
  if (cleanup_timer_.IsRunning()) {
    return;
  }
  cleanup_timer_.Start(FROM_HERE, base::Seconds(5),
                       base::BindOnce(&GlicTabFaviconObserver::DoCleanup,
                                      base::Unretained(this)));
}

void GlicTabFaviconObserver::DoCleanup() {
  for (tabs::TabHandle handle : std::exchange(pending_cleanup_, {})) {
    auto iter = observers_.find(handle);
    if (iter != observers_.end()) {
      if (!iter->second->HasReceivers()) {
        observers_.erase(iter);
      }
    }
  }
}

}  // namespace glic
