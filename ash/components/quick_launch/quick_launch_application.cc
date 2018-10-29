// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/quick_launch/quick_launch_application.h"

#include "ash/public/cpp/ash_client.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "mash/public/mojom/launchable.mojom.h"
#include "services/catalog/public/mojom/catalog.mojom.h"
#include "services/catalog/public/mojom/constants.mojom.h"
#include "services/service_manager/public/c/main.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/service_runner.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/views/background.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/mus/aura_init.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "url/gurl.h"

namespace quick_launch {
namespace {

class QuickLaunchUI : public views::WidgetDelegateView,
                      public views::TextfieldController {
 public:
  QuickLaunchUI(QuickLaunchApplication* quick_launch,
                service_manager::Connector* connector,
                catalog::mojom::CatalogPtr catalog)
      : quick_launch_(quick_launch),
        connector_(connector),
        prompt_(new views::Textfield),
        catalog_(std::move(catalog)) {
    SetBackground(views::CreateStandardPanelBackground());
    prompt_->set_controller(this);
    AddChildView(prompt_);

    UpdateEntries();
  }
  ~QuickLaunchUI() override { quick_launch_->Quit(); }

 private:
  // Overridden from views::WidgetDelegate:
  base::string16 GetWindowTitle() const override {
    // TODO(beng): use resources.
    return base::ASCIIToUTF16("QuickLaunch");
  }

  // Overridden from views::View:
  void Layout() override {
    gfx::Rect bounds = GetLocalBounds();
    bounds.Inset(5, 5);
    prompt_->SetBoundsRect(bounds);
  }
  gfx::Size CalculatePreferredSize() const override {
    gfx::Size ps = prompt_->GetPreferredSize();
    ps.Enlarge(500, 10);
    return ps;
  }

  // Overridden from views::TextFieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override {
    if (key_event.type() != ui::ET_KEY_PRESSED)
      return false;

    // The user didn't like our suggestion, don't make another until they
    // type another character.
    suggestion_rejected_ = key_event.key_code() == ui::VKEY_BACK ||
                           key_event.key_code() == ui::VKEY_DELETE;
    if (key_event.key_code() == ui::VKEY_RETURN) {
      Launch(Canonicalize(prompt_->text()), key_event.IsControlDown());
      prompt_->SetText(base::string16());
      UpdateEntries();
    }
    return false;
  }

  void ContentsChanged(views::Textfield* sender,
                       const base::string16& new_contents) override {
    // Don't keep making a suggestion if the user didn't like what we offered.
    if (suggestion_rejected_)
      return;

    if (new_contents.empty())
      return;

    // TODO(beng): it'd be nice if we persisted some history/scoring here.
    for (const auto& name : app_names_) {
      if (base::StartsWith(name, new_contents,
                           base::CompareCase::INSENSITIVE_ASCII)) {
        base::string16 suffix = name;
        base::ReplaceSubstringsAfterOffset(&suffix, 0, new_contents,
                                           base::string16());
        gfx::Range range(static_cast<uint32_t>(new_contents.size()),
                         static_cast<uint32_t>(name.size()));
        prompt_->SetText(name);
        prompt_->SelectRange(range);
        break;
      }
    }
  }

  std::string Canonicalize(const base::string16& input) const {
    base::string16 working;
    base::TrimWhitespace(input, base::TRIM_ALL, &working);
    GURL url(working);
    if (url.scheme() != "service" && url.scheme() != "exe")
      working = base::ASCIIToUTF16("") + working;
    return base::UTF16ToUTF8(working);
  }

  void UpdateEntries() {
    catalog_->GetEntriesProvidingCapability(
        "mash:launchable",
        base::BindRepeating(&QuickLaunchUI::OnGotCatalogEntries,
                            base::Unretained(this)));
  }

  void OnGotCatalogEntries(std::vector<catalog::mojom::EntryPtr> entries) {
    for (const auto& entry : entries)
      app_names_.insert(base::UTF8ToUTF16(entry->name));
  }

  void Launch(const std::string& name, bool new_window) {
    // TODO(jamescook): Start the service by name. Most services don't
    // support the Launchable interface any more.
    ::mash::mojom::LaunchablePtr launchable;
    connector_->BindInterface(name, &launchable);
    launchable->Launch(mash::mojom::kWindow,
                       new_window ? mash::mojom::LaunchMode::MAKE_NEW
                                  : mash::mojom::LaunchMode::REUSE);
  }

  QuickLaunchApplication* quick_launch_;
  service_manager::Connector* connector_;
  views::Textfield* prompt_;
  catalog::mojom::CatalogPtr catalog_;
  std::set<base::string16> app_names_;
  bool suggestion_rejected_ = false;

  DISALLOW_COPY_AND_ASSIGN(QuickLaunchUI);
};

}  // namespace

QuickLaunchApplication::QuickLaunchApplication() = default;

QuickLaunchApplication::~QuickLaunchApplication() {
  if (window_)
    window_->CloseNow();
}

void QuickLaunchApplication::Quit() {
  window_ = nullptr;
  context()->QuitNow();
}

void QuickLaunchApplication::OnStart() {
  // If AuraInit was unable to initialize there is no longer a peer connection.
  // The ServiceManager is in the process of shutting down, however we haven't
  // been notified yet. Close our ServiceContext and shutdown.
  views::AuraInit::InitParams params;
  params.connector = context()->connector();
  params.identity = context()->identity();
  params.register_path_provider = running_standalone_;
  params.use_accessibility_host = true;
  aura_init_ = views::AuraInit::Create(params);
  if (!aura_init_) {
    context()->QuitNow();
    return;
  }

  // Register as a client of the window manager.
  ash::ash_client::Init();

  catalog::mojom::CatalogPtr catalog;
  context()->connector()->BindInterface(catalog::mojom::kServiceName, &catalog);

  window_ = views::Widget::CreateWindowWithContextAndBounds(
      new QuickLaunchUI(this, context()->connector(), std::move(catalog)),
      nullptr, gfx::Rect(10, 640, 0, 0));
  window_->GetNativeWindow()->GetHost()->window()->SetName("QuickLaunch");
  window_->Show();
}

void QuickLaunchApplication::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

}  // namespace quick_launch
