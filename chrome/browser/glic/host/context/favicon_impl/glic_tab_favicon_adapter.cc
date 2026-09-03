// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/favicon_impl/glic_tab_favicon_adapter.h"

#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#endif

namespace glic {
namespace {

#if BUILDFLAG(IS_ANDROID)
constexpr float kFaviconScale = 1.0f;
#else
constexpr float kFaviconScale = 2.0f;
#endif

}  // namespace

FaviconData::FaviconData() = default;
FaviconData::FaviconData(const FaviconData&) = default;
FaviconData::~FaviconData() = default;

FaviconData::FaviconData(gfx::Image image) : image_(std::move(image)) {}
FaviconData::FaviconData(SkBitmap bitmap) : bitmap_(std::move(bitmap)) {}

// static
FaviconData FaviconData::FromImage(gfx::Image image) {
  return FaviconData(std::move(image));
}

// static
FaviconData FaviconData::FromBitmap(SkBitmap bitmap) {
  return FaviconData(std::move(bitmap));
}

// static
FaviconData FaviconData::FromWebContents(content::WebContents& web_contents) {
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

const SkBitmap& FaviconData::GetBitmap() const {
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

bool FaviconData::operator==(const FaviconData& other) const {
  // Ignore image if empty, they're not always available.
  if (!image_.IsEmpty() && image_ == other.image_) {
    return true;
  }
  return FaviconEquals(GetBitmap(), other.GetBitmap());
}

FaviconNotifier::FaviconNotifier() = default;
FaviconNotifier::~FaviconNotifier() = default;

void FaviconNotifier::SetFaviconAndNotify(const FaviconData& favicon_data) {
  if (favicon_data == favicon_data_) {
    return;
  }
  favicon_data_ = favicon_data;
  NotifyFaviconChanged();
}

void FaviconNotifier::Subscribe(
    ::mojo::PendingRemote<mojom::TabFaviconHandler> receiver) {
  mojo::Remote<mojom::TabFaviconHandler> new_remote;
  new_remote.Bind(std::move(receiver));
  const SkBitmap& bitmap = favicon_data_.GetBitmap();
  new_remote->OnTabFaviconChanged(bitmap.drawsNothing() ? SkBitmap() : bitmap);
  receivers_.Add(std::move(new_remote));
}

void FaviconNotifier::NotifyFaviconChanged() {
  const SkBitmap& bitmap = favicon_data_.GetBitmap();
  const SkBitmap sent_bitmap = bitmap.drawsNothing() ? SkBitmap() : bitmap;
  for (auto& receiver : receivers_) {
    receiver->OnTabFaviconChanged(sent_bitmap);
  }
}

FaviconAdapter::FaviconAdapter(tabs::TabInterface* tab,
                               FaviconNotifier* notifier)
    : content::WebContentsObserver(tab->GetContents()),
      tab_(tab),
      notifier_(notifier) {}

void FaviconAdapter::PrimaryPageChanged(content::Page& page) {
  if (!web_contents()) {
    return;
  }
  // On Android, TabFavicon::Observer only notifies when a new, non-empty
  // favicon is successfully loaded. It does not notify when navigation starts
  // or commits, or if the new page has no favicon. We must handle
  // PrimaryPageChanged here to proactively clear or update the favicon state
  // on navigation, otherwise Glic would show stale favicons from the previous
  // page.
  notifier_->SetFaviconAndNotify(FaviconData::FromWebContents(*web_contents()));
}

}  // namespace glic
