// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/selection/chrome_selection_dropdown_menu_delegate.h"


#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/devtools/devtools_availability_checker.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/menus/simple_menu_model.h"

#if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
#include "chrome/browser/extensions/extension_menu_model_android.h"
#endif

namespace android {

namespace {

#if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
using BaseSelectionDropdownMenuModel = extensions::ExtensionMenuModel;
#else
using BaseSelectionDropdownMenuModel = ui::SimpleMenuModel;
#endif

class ChromeSelectionDropdownMenuModel : public BaseSelectionDropdownMenuModel
#if !BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
    ,
                                         public ui::SimpleMenuModel::Delegate
#endif
{
 public:
  ChromeSelectionDropdownMenuModel(content::RenderFrameHost& render_frame_host,
                                   const content::ContextMenuParams& params)
#if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
      : BaseSelectionDropdownMenuModel(render_frame_host, params),
#else
      : BaseSelectionDropdownMenuModel(this),
#endif
        rfh_id_(render_frame_host.GetGlobalId()),
        params_(params) {
  }

  ~ChromeSelectionDropdownMenuModel() override = default;

  // ui::SimpleMenuModel::Delegate overrides:
  void ExecuteCommand(int command_id, int event_flags) override {
    if (command_id == IDC_CONTENT_CONTEXT_INSPECTELEMENT) {
      auto* rfh = content::RenderFrameHost::FromID(rfh_id_);
      if (rfh && rfh->IsRenderFrameLive()) {
        DevToolsWindow::InspectElement(rfh, params_.x, params_.y);
      }
      return;
    }
#if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
    BaseSelectionDropdownMenuModel::ExecuteCommand(command_id, event_flags);
#endif
  }

  bool IsCommandIdChecked(int command_id) const override {
    if (command_id == IDC_CONTENT_CONTEXT_INSPECTELEMENT) {
      return false;
    }
#if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
    return BaseSelectionDropdownMenuModel::IsCommandIdChecked(command_id);
#else
    return false;
#endif
  }

  bool IsCommandIdEnabled(int command_id) const override {
    if (command_id == IDC_CONTENT_CONTEXT_INSPECTELEMENT) {
      return true;
    }
#if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
    return BaseSelectionDropdownMenuModel::IsCommandIdEnabled(command_id);
#else
    return false;
#endif
  }

  bool IsCommandIdVisible(int command_id) const override {
    if (command_id == IDC_CONTENT_CONTEXT_INSPECTELEMENT) {
      return true;
    }
#if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
    return BaseSelectionDropdownMenuModel::IsCommandIdVisible(command_id);
#else
    return false;
#endif
  }

 private:
  content::GlobalRenderFrameHostId rfh_id_;
  content::ContextMenuParams params_;
};

}  // namespace

ChromeSelectionDropdownMenuDelegate::ChromeSelectionDropdownMenuDelegate() =
    default;

ChromeSelectionDropdownMenuDelegate::~ChromeSelectionDropdownMenuDelegate() =
    default;

// SelectionPopupDelegate implementation.
std::unique_ptr<ui::MenuModel>
ChromeSelectionDropdownMenuDelegate::GetSelectionPopupExtraItems(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  Profile* profile =
      Profile::FromBrowserContext(render_frame_host.GetBrowserContext());
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host);
  bool is_devtools_allowed = IsInspectionAllowed(profile, web_contents);

#if !BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
  if (!is_devtools_allowed) {
    return nullptr;
  }
#endif

  std::unique_ptr<ChromeSelectionDropdownMenuModel> model =
      std::make_unique<ChromeSelectionDropdownMenuModel>(render_frame_host,
                                                         params);
#if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
  model->PopulateModel();
#endif

  if (is_devtools_allowed) {
#if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
    if (model->GetItemCount() > 0) {
      model->AddSeparator(ui::NORMAL_SEPARATOR);
    }
#endif
    model->AddItemWithStringId(IDC_CONTENT_CONTEXT_INSPECTELEMENT,
                               IDS_INSPECT_ELEMENT_ANDROID);
  }

  return model;
}
}  // namespace android
