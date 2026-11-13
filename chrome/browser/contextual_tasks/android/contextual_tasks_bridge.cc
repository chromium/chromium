// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/android/contextual_tasks_bridge.h"

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/contextual_tasks/active_task_context_provider.h"
#include "chrome/browser/contextual_tasks/active_task_context_provider_impl.h"
#include "chrome/browser/contextual_tasks/contextual_tasks.mojom.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/contextual_tasks/entry_point_eligibility_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/webui_url_constants.h"
#include "components/browser_ui/util/android/url_constants.h"
#include "components/contextual_tasks/public/features.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "third_party/jni_zero/jni_zero.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after headers that declare FromJniType / ToJniType
#include "chrome/browser/contextual_tasks/jni_headers/ContextualTasksBridge_jni.h"

namespace contextual_tasks {

namespace {

using AimEntryPoint = omnibox::ChromeAimEntryPoint;
using InvocationSource = lens::LensOverlayInvocationSource;

// Maps the invocation source of an "Ask Google" entry point to the AIM entry
// point reported to the side panel. Desktop values are reused until dedicated
// Android values are added to the proto.
AimEntryPoint AimEntryPointForInvocationSource(InvocationSource source) {
  switch (source) {
    case InvocationSource::kAppMenu:
    case InvocationSource::kAppMenuAskGoogle:
      return AimEntryPoint::DESKTOP_CHROME_APP_MENU_ASK_GOOGLE;
    case InvocationSource::kContentAreaContextMenuPage:
    case InvocationSource::kContentAreaContextMenuPageAskGoogle:
    case InvocationSource::kContentAreaContextMenuImage:
      return AimEntryPoint::DESKTOP_CHROME_PAGE_CONTEXT_MENU_ASK_GOOGLE;
    case InvocationSource::kOmnibox:
    case InvocationSource::kOmniboxPageAction:
    case InvocationSource::kOmniboxContextualSuggestion:
      return AimEntryPoint::DESKTOP_CHROME_COBROWSE_OMNIBOX_ACTION;
    case InvocationSource::kToolbar:
    case InvocationSource::kCobrowseToolbarButton:
      return AimEntryPoint::DESKTOP_CHROME_COBROWSE_TOOLBAR_BUTTON;
    case InvocationSource::kCobrowsePinnedToolbarButton:
      return AimEntryPoint::DESKTOP_CHROME_COBROWSE_PINNED_TOOLBAR_BUTTON;
    default:
      return AimEntryPoint::UNKNOWN_AIM_ENTRY_POINT;
  }
}

// Returns the window-scoped panel controller for `web_contents`, if any.
ContextualTasksPanelController* GetPanelController(
    content::WebContents* web_contents) {
  tabs::TabInterface* tab =
      web_contents ? tabs::TabInterface::MaybeGetFromContents(web_contents)
                   : nullptr;
  BrowserWindowInterface* browser_window =
      tab ? tab->GetBrowserWindowInterface() : nullptr;
  return browser_window ? ContextualTasksPanelController::From(browser_window)
                        : nullptr;
}

// Holds resolved task reference, ID, and optional task state.
struct ResolvedTaskInfo {
  raw_ptr<ContextualTasksService> service;
  base::Uuid task_id;
  std::optional<ContextualTask> task;
};

ResolvedTaskInfo ResolveTaskInfoForWebContents(
    content::WebContents* web_contents) {
  ResolvedTaskInfo info;
  if (!web_contents) {
    return info;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  info.service = ContextualTasksServiceFactory::GetForProfile(profile);
  if (!info.service) {
    return info;
  }

  SessionID tab_id = sessions::SessionTabHelper::IdForTab(web_contents);
  if (tab_id.is_valid()) {
    info.task = info.service->GetContextualTaskForTab(tab_id);
    if (info.task) {
      info.task_id = info.task->GetTaskId();
      return info;
    }
  }

  const GURL& url = web_contents->GetLastCommittedURL();
  if (contextual_tasks::ContextualTasksUiService::IsContextualTasksUrl(url)) {
    info.task_id =
        contextual_tasks::ContextualTasksUiService::GetTaskIdFromUrl(url);
  }

  return info;
}

}  // namespace

DEFINE_USER_DATA(ContextualTasksBridge);

static int64_t JNI_ContextualTasksBridge_Init(
    const jni_zero::JavaRef<jobject>& caller,
    BrowserWindowInterface* browser_window,
    Profile* profile) {
  CHECK(browser_window);
  return reinterpret_cast<intptr_t>(
      new ContextualTasksBridge(caller, browser_window, profile));
}

// static
ContextualTasksBridge* ContextualTasksBridge::From(
    BrowserWindowInterface* window) {
  if (!window) {
    return nullptr;
  }
  return Get(window->GetUnownedUserDataHost());
}

ContextualTasksBridge::ContextualTasksBridge(
    const jni_zero::JavaRef<jobject>& obj,
    BrowserWindowInterface* browser_window,
    Profile* profile)
    : profile_(profile),
      contextual_tasks_ui_service_(
          contextual_tasks::ContextualTasksUiServiceFactory::
              GetForBrowserContext(profile)),
      java_obj_(obj),
      scoped_unowned_user_data_(browser_window->GetUnownedUserDataHost(),
                                *this) {
  CHECK(contextual_tasks_ui_service_);
  entry_point_eligibility_manager_ =
      GetUserDataFactory().CreateInstance<EntryPointEligibilityManager>(
          *browser_window, browser_window);
  active_task_context_provider_ =
      GetUserDataFactory().CreateInstance<ActiveTaskContextProviderImpl>(
          *browser_window, browser_window,
          ContextualTasksServiceFactory::GetForProfile(profile));
  controller_ =
      GetUserDataFactory().CreateInstance<ContextualTasksSidePanelCoordinator>(
          *browser_window, browser_window, active_task_context_provider_.get(),
          entry_point_eligibility_manager_.get());
}

ContextualTasksBridge::~ContextualTasksBridge() = default;

// static
bool ContextualTasksBridge::ShouldShowAskGoogleEntryPoints(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return false;
  }
  if (!IsContextualTasksUIEnabled()) {
    return false;
  }
  // Native pages (e.g. the NTP) have no shareable page content.
  const GURL& url = web_contents->GetLastCommittedURL();
  if (!IsUrlValidForContextSharing(url) ||
      url.SchemeIs(browser_ui::kChromeUINativeScheme)) {
    return false;
  }
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile || profile->IsOffTheRecord()) {
    return false;
  }
  if (!search::DefaultSearchProviderIsGoogle(profile)) {
    return false;
  }
  if (!IsContextualTasksEntryPointsEnabled() &&
      !AreContextualTasksUpdatedEntryPointsEnabled(profile)) {
    return false;
  }
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents);
  if (!tab || !tab->GetBrowserWindowInterface()) {
    return false;
  }
  auto* eligibility_manager =
      EntryPointEligibilityManager::From(tab->GetBrowserWindowInterface());
  if (!eligibility_manager || !eligibility_manager->AreEntryPointsEligible()) {
    return false;
  }
  return true;
}

// static
void ContextualTasksBridge::OpenAskGoogleAboutThisPage(
    content::WebContents* web_contents,
    InvocationSource invocation_source) {
  if (!ShouldShowAskGoogleEntryPoints(web_contents)) {
    return;
  }
  auto* controller = GetPanelController(web_contents);
  if (!controller) {
    return;
  }
  controller->Show(/*transition_from_tab=*/false,
                   AimEntryPointForInvocationSource(invocation_source));
}

// static
void ContextualTasksBridge::ToggleAskGoogleSidePanel(
    content::WebContents* web_contents,
    InvocationSource invocation_source) {
  auto* controller = GetPanelController(web_contents);
  if (controller && controller->IsPanelOpenForContextualTask()) {
    controller->Close();
    return;
  }
  OpenAskGoogleAboutThisPage(web_contents, invocation_source);
}

void ContextualTasksBridge::Destroy() {
  delete this;
}

void ContextualTasksBridge::UndoClose() {
  if (controller_) {
    controller_->Show();
  }
}

void ContextualTasksBridge::StartPlatformVoiceRecognition() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContextualTasksBridge_startVoiceRecognition(env, java_obj_);
}

void ContextualTasksBridge::OnVoiceTranscribed(const std::string& query) {
  contextual_tasks_ui_service_->OnVoiceTranscribed(query);
}

void ContextualTasksBridge::NotifyShowUndoSnackbar() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContextualTasksBridge_showUndoSnackbar(env, java_obj_);
}

void ContextualTasksBridge::NotifyOpenFeedbackUi(const GURL& page_url) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContextualTasksBridge_openFeedbackUi(env, java_obj_, page_url.spec());
}

// static
void ContextualTasksBridge::GetTaskTitleForTab(
    content::WebContents* web_contents,
    base::OnceCallback<void(std::string)> callback) {
  ResolvedTaskInfo info = ResolveTaskInfoForWebContents(web_contents);

  if (info.task || !info.service || !info.task_id.is_valid()) {
    std::string title = info.task ? info.task->GetTitle() : std::string();
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(title)));
    return;
  }

  info.service->GetTaskById(
      info.task_id,
      base::BindOnce(
          [](base::OnceCallback<void(std::string)> callback,
             std::optional<ContextualTask> task) {
            std::move(callback).Run(task ? task->GetTitle() : std::string());
          },
          std::move(callback)));
}

static void JNI_ContextualTasksBridge_GetTaskTitleForTab(
    content::WebContents* web_contents,
    base::OnceCallback<void(std::string)> j_callback) {
  ContextualTasksBridge::GetTaskTitleForTab(web_contents,
                                            std::move(j_callback));
}

static bool JNI_ContextualTasksBridge_IsPanelOpen(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return false;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  contextual_tasks::ContextualTasksService* contextual_tasks_service =
      contextual_tasks::ContextualTasksServiceFactory::GetForProfile(profile);
  if (!contextual_tasks_service) {
    return false;
  }

  SessionID tab_id = sessions::SessionTabHelper::IdForTab(web_contents);
  if (!tab_id.is_valid()) {
    return false;
  }

  return contextual_tasks_service->GetContextualTaskForTab(tab_id).has_value();
}

// static
ui::UserDataFactoryWithOwner<BrowserWindowInterface>&
ContextualTasksBridge::GetUserDataFactory() {
  static base::NoDestructor<
      ui::UserDataFactoryWithOwner<BrowserWindowInterface>>
      factory;
  return *factory;
}

}  // namespace contextual_tasks

DEFINE_JNI(ContextualTasksBridge)
