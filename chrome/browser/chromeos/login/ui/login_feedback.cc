// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/login_feedback.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/api/feedback_private/feedback_private_api.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"

namespace chromeos {

namespace {

extensions::ComponentLoader* GetComponentLoader(
    content::BrowserContext* context) {
  extensions::ExtensionSystem* extension_system =
      extensions::ExtensionSystem::Get(context);
  extensions::ExtensionService* extension_service =
      extension_system->extension_service();
  return extension_service->component_loader();
}

}  // namespace

// Ensures that the feedback extension is loaded on the signin profile and
// invokes the callback when the extension is ready to use. Unload the
// extension and delete itself when the extension's background page shuts down.
class FeedbackExtensionLoader : public extensions::ExtensionRegistryObserver {
 public:
  explicit FeedbackExtensionLoader(Profile* profile);
  ~FeedbackExtensionLoader() override;

  // Loads the feedback extension on the given profile and invokes
  // |on_ready_callback| when it is ready.
  void Load(base::OnceClosure on_ready_callback);

 private:
  // extensions::ExtensionRegistryObserver overrides:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;

  void RunOnReadyCallback();

  Profile* const profile_;

  // Extension registry for the login profile lives till the login profile is
  // destructed. This will always happen after the signin screen web UI has
  // been destructed (which destructs us in the process). Hence this pointer
  // will outlive us.
  extensions::ExtensionRegistry* extension_registry_;

  base::OnceClosure on_ready_callback_;

  DISALLOW_COPY_AND_ASSIGN(FeedbackExtensionLoader);
};

FeedbackExtensionLoader::FeedbackExtensionLoader(Profile* profile)
    : profile_(profile),
      extension_registry_(extensions::ExtensionRegistry::Get(profile)) {}

FeedbackExtensionLoader::~FeedbackExtensionLoader() {
  extension_registry_->RemoveObserver(this);
  // The extension will be removed via a JS FeedbackPrivate API call to
  // indicate when it is done,
}

void FeedbackExtensionLoader::Load(base::OnceClosure on_ready_callback) {
  DCHECK(!on_ready_callback.is_null());
  on_ready_callback_ = std::move(on_ready_callback);
  if (extension_registry_->enabled_extensions().Contains(
          extension_misc::kFeedbackExtensionId)) {
    RunOnReadyCallback();
    return;
  }
  extension_registry_->AddObserver(this);
  extensions::ComponentLoader* component_loader = GetComponentLoader(profile_);
  if (!component_loader->Exists(extension_misc::kFeedbackExtensionId)) {
    component_loader->Add(IDR_FEEDBACK_MANIFEST,
                          base::FilePath(FILE_PATH_LITERAL("feedback")));
  }
}

void FeedbackExtensionLoader::RunOnReadyCallback() {
  DCHECK(!on_ready_callback_.is_null());
  std::move(on_ready_callback_).Run();
  extension_registry_->RemoveObserver(this);
}

void FeedbackExtensionLoader::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  if (extension->id() == extension_misc::kFeedbackExtensionId) {
    RunOnReadyCallback();
  }
}

////////////////////////////////////////////////////////////////////////////////
// LoginFeedback::FeedbackWindowHandler

class LoginFeedback::FeedbackWindowHandler
    : public extensions::AppWindowRegistry::Observer {
 public:
  explicit FeedbackWindowHandler(LoginFeedback* owner);
  ~FeedbackWindowHandler() override;

  bool HasFeedbackAppWindow() const;

  // extensions::AppWindowRegistry::Observer
  void OnAppWindowRemoved(extensions::AppWindow* app_window) override;

 private:
  LoginFeedback* const owner_;
  extensions::AppWindowRegistry* const window_registry_;

  DISALLOW_COPY_AND_ASSIGN(FeedbackWindowHandler);
};

LoginFeedback::FeedbackWindowHandler::FeedbackWindowHandler(
    LoginFeedback* owner)
    : owner_(owner),
      window_registry_(extensions::AppWindowRegistry::Get(owner_->profile_)) {
  window_registry_->AddObserver(this);
}

LoginFeedback::FeedbackWindowHandler::~FeedbackWindowHandler() {
  window_registry_->RemoveObserver(this);
}

bool LoginFeedback::FeedbackWindowHandler::HasFeedbackAppWindow() const {
  return !window_registry_
              ->GetAppWindowsForApp(extension_misc::kFeedbackExtensionId)
              .empty();
}

void LoginFeedback::FeedbackWindowHandler::OnAppWindowRemoved(
    extensions::AppWindow* app_window) {
  if (app_window->extension_id() != extension_misc::kFeedbackExtensionId)
    return;

  if (!HasFeedbackAppWindow())
    owner_->OnFeedbackFinished();
}

////////////////////////////////////////////////////////////////////////////////
// LoginFeedback

LoginFeedback::LoginFeedback(Profile* signin_profile)
    : profile_(signin_profile) {}

LoginFeedback::~LoginFeedback() {}

void LoginFeedback::Request(const std::string& description,
                            base::OnceClosure finished_callback) {
  description_ = description;
  finished_callback_ = std::move(finished_callback);
  feedback_window_handler_.reset(new FeedbackWindowHandler(this));

  // Do not call EnsureFeedbackUI() immediately. Otherwise, event listener is
  // possibly registered before extension installation is complete in
  // EventRouter::DispatchEventWithLazyListener() which possibly causes a race
  // condition.

  feedback_extension_loader_ =
      std::make_unique<FeedbackExtensionLoader>(profile_);
  feedback_extension_loader_->Load(base::BindOnce(
      &LoginFeedback::EnsureFeedbackUI, weak_factory_.GetWeakPtr()));
}

void LoginFeedback::EnsureFeedbackUI() {
  // Bail if any feedback app window is opened.
  if (feedback_window_handler_->HasFeedbackAppWindow())
    return;

  extensions::FeedbackPrivateAPI* api =
      extensions::FeedbackPrivateAPI::GetFactoryInstance()->Get(profile_);
  api->RequestFeedbackForFlow(
      description_, std::string(), "Login", std::string(), GURL(),
      extensions::api::feedback_private::FeedbackFlow::FEEDBACK_FLOW_LOGIN);

  // Make sure there is a feedback app window opened.
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&LoginFeedback::EnsureFeedbackUI,
                     weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(1));
}

void LoginFeedback::OnFeedbackFinished() {
  if (!finished_callback_.is_null())
    std::move(finished_callback_).Run();
}

}  // namespace chromeos
