// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_MEDIA_APP_CHROME_MEDIA_APP_UI_DELEGATE_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_MEDIA_APP_CHROME_MEDIA_APP_UI_DELEGATE_H_

#include "ash/webui/media_app_ui/media_app_ui_delegate.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {
class WebUI;
}

/**
 * Implementation of the MediaAppUiDelegate interface. Provides the media app
 * code in chromeos/ with functions that only exist in chrome/.
 */
class ChromeMediaAppUIDelegate : public ash::MediaAppUIDelegate {
 public:
  explicit ChromeMediaAppUIDelegate(content::WebUI* web_ui);

  ChromeMediaAppUIDelegate(const ChromeMediaAppUIDelegate&) = delete;
  ChromeMediaAppUIDelegate& operator=(const ChromeMediaAppUIDelegate&) = delete;
  ~ChromeMediaAppUIDelegate() override;

  // MediaAppUIDelegate:
  base::WeakPtr<ash::MediaAppUIDelegate> GetWeakPtr() override;
  absl::optional<std::string> OpenFeedbackDialog() override;
  void ToggleBrowserFullscreenMode() override;
  void EditFileInPhotos(
      absl::optional<storage::FileSystemURL> url,
      const std::string& mime_type,
      base::OnceCallback<void()> edit_in_photos_callback) override;

 private:
  content::WebUI* web_ui_;  // Owns |this|.
  base::WeakPtrFactory<ChromeMediaAppUIDelegate> weak_factory_{this};
};

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_MEDIA_APP_CHROME_MEDIA_APP_UI_DELEGATE_H_
