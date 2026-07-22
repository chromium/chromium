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
#include "printing/buildflags/buildflags.h"
#include "ui/menus/simple_menu_model.h"

#if BUILDFLAG(ENABLE_PRINTING)
#include "base/feature_list.h"
#include "base/metrics/user_metrics.h"
#include "chrome/android/chrome_jni_headers/TabPrinter_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "third_party/blink/public/mojom/forms/form_control_type.mojom-shared.h"
#endif

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

#if BUILDFLAG(ENABLE_PRINTING)
    if (command_id == IDC_PRINT) {
      base::RecordAction(
          base::UserMetricsAction("MobileSelectionDropdown.Print"));
      auto* rfh = content::RenderFrameHost::FromID(rfh_id_);
      if (rfh && rfh->IsActive() && rfh->IsRenderFrameLive()) {
        content::WebContents* web_contents =
            content::WebContents::FromRenderFrameHost(rfh);
        CHECK(web_contents);
        if (TabAndroid* tab = TabAndroid::FromWebContents(web_contents)) {
          JNIEnv* env = base::android::AttachCurrentThread();
          printing::Java_TabPrinter_printSelection(
              env, tab->GetJavaObject(), rfh->GetJavaRenderFrameHost());
        }
      }
      return;
    }
#endif

#if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
    BaseSelectionDropdownMenuModel::ExecuteCommand(command_id, event_flags);
#endif
  }

  bool IsChromeOwnedCommand(int command_id) const {
    if (command_id == IDC_CONTENT_CONTEXT_INSPECTELEMENT) {
      return true;
    }
#if BUILDFLAG(ENABLE_PRINTING)
    if (command_id == IDC_PRINT) {
      return true;
    }
#endif
    return false;
  }

  bool IsCommandIdChecked(int command_id) const override {
    if (IsChromeOwnedCommand(command_id)) {
      return false;
    }

#if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
    return BaseSelectionDropdownMenuModel::IsCommandIdChecked(command_id);
#else
    return false;
#endif
  }

  bool IsCommandIdEnabled(int command_id) const override {
    if (IsChromeOwnedCommand(command_id)) {
      return true;
    }

#if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
    return BaseSelectionDropdownMenuModel::IsCommandIdEnabled(command_id);
#else
    return false;
#endif
  }

  bool IsCommandIdVisible(int command_id) const override {
    if (IsChromeOwnedCommand(command_id)) {
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
  [[maybe_unused]] bool should_show_print_item = false;
#if BUILDFLAG(ENABLE_PRINTING)
  should_show_print_item =
      base::FeatureList::IsEnabled(chrome::android::kPrintSelectionMenu) &&
      profile && profile->GetPrefs()->GetBoolean(prefs::kPrintingEnabled) &&
      !params.selection_text.empty() &&
      params.form_control_type != blink::mojom::FormControlType::kInputPassword;
#endif

#if !BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
  if (!is_devtools_allowed && !should_show_print_item) {
    return nullptr;
  }
#endif

  std::unique_ptr<ChromeSelectionDropdownMenuModel> model =
      std::make_unique<ChromeSelectionDropdownMenuModel>(render_frame_host,
                                                         params);
#if BUILDFLAG(ENABLE_PRINTING)
  if (should_show_print_item) {
    model->AddItemWithStringId(IDC_PRINT,
                               IDS_CONTEXTMENU_PRINT_SELECTION_DROPDOWN);
  }
#endif

#if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS)
  if (model->GetItemCount() > 0) {
    model->AddSeparator(ui::NORMAL_SEPARATOR);
  }
  model->PopulateModel();
#endif

  if (is_devtools_allowed) {
#if BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS) || BUILDFLAG(ENABLE_PRINTING)
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
