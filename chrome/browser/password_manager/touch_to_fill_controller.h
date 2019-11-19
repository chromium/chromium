// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_TOUCH_TO_FILL_CONTROLLER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_TOUCH_TO_FILL_CONTROLLER_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/util/type_safety/pass_key.h"
#include "chrome/browser/touch_to_fill/touch_to_fill_view.h"
#include "chrome/browser/touch_to_fill/touch_to_fill_view_factory.h"
#include "components/favicon_base/favicon_types.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

namespace favicon {
class FaviconService;
}  // namespace favicon

namespace password_manager {
class PasswordManagerDriver;
struct CredentialPair;
}  // namespace password_manager

class ChromePasswordManagerClient;

class TouchToFillController {
 public:
  // The action a user took when interacting with the Touch To Fill sheet.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. Needs to stay in sync with
  // TouchToFill.UserAction in enums.xml and UserAction in
  // TouchToFillComponent.java.
  //
  // TODO(crbug.com/1013134): De-duplicate the Java and C++ enum.
  enum class UserAction {
    kSelectedCredential = 0,
    kDismissed = 1,
    kSelectedManagePasswords = 2,
  };

  // No-op constructor for tests.
  explicit TouchToFillController(
      util::PassKey<class TouchToFillControllerTest>);
  TouchToFillController(ChromePasswordManagerClient* web_contents,
                        favicon::FaviconService* favicon_service);
  TouchToFillController(const TouchToFillController&) = delete;
  TouchToFillController& operator=(const TouchToFillController&) = delete;
  ~TouchToFillController();

  // Instructs the controller to show the provided |credentials| to the user.
  void Show(base::span<const password_manager::CredentialPair> credentials,
            base::WeakPtr<password_manager::PasswordManagerDriver> driver);

  // Informs the controller that the user has made a selection. Invokes both
  // FillSuggestion() and TouchToFillDismissed() on |driver_|. No-op if invoked
  // repeatedly.
  void OnCredentialSelected(const password_manager::CredentialPair& credential);

  // Informs the controller that the user has tapped the "Manage Passwords"
  // button. This will open the password preferences.
  void OnManagePasswordsSelected();

  // Informs the controller that the user has dismissed the sheet. Invokes
  // TouchToFillDismissed() on |driver_|. No-op if invoked repeatedly.
  void OnDismiss();

  // The web page view containing the focused field.
  gfx::NativeView GetNativeView();

  // Obtains a favicon for the origin and invokes the callback with a favicon
  // image or with an empty image if a favicon could not be retrieved.
  void FetchFavicon(const GURL& credential_origin,
                    const GURL& frame_origin,
                    int desired_size_in_pixel,
                    base::OnceCallback<void(const gfx::Image&)> callback);

#if defined(UNIT_TEST)
  void set_view(std::unique_ptr<TouchToFillView> view) {
    view_ = std::move(view);
  }
#endif

 private:
  // Weak pointer to the ChromePasswordManagerClient this class is tied to.
  ChromePasswordManagerClient* password_client_ = nullptr;

  // Driver passed to the latest invocation of Show(). Gets cleared when
  // OnCredentialSelected() or OnDismissed() gets called.
  base::WeakPtr<password_manager::PasswordManagerDriver> driver_;

  // The favicon service used to retrieve icons for a given origin.
  favicon::FaviconService* favicon_service_ = nullptr;

  ukm::SourceId source_id_ = ukm::kInvalidSourceId;

  // Used to track requested favicons. On destruction, requests are cancelled.
  base::CancelableTaskTracker favicon_tracker_;

  // View used to communicate with the Android frontend. Lazily instantiated so
  // that it can be injected by tests.
  std::unique_ptr<TouchToFillView> view_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_TOUCH_TO_FILL_CONTROLLER_H_
