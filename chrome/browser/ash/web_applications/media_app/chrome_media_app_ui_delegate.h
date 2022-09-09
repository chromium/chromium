// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_MEDIA_APP_CHROME_MEDIA_APP_UI_DELEGATE_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_MEDIA_APP_CHROME_MEDIA_APP_UI_DELEGATE_H_

#include "ash/webui/media_app_ui/media_app_ui_delegate.h"
#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_transfer_token.mojom.h"

namespace ash {
class HatsNotificationController;
}

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
  absl::optional<std::string> OpenFeedbackDialog() override;
  void ToggleBrowserFullscreenMode() override;
  void MaybeTriggerPdfHats() override;
  void IsFileArcWritable(
      mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token,
      base::OnceCallback<void(bool)> is_file_arc_writable_callback) override;
  void EditInPhotos(
      mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken> token,
      const std::string& mime_type,
      base::OnceCallback<void()> edit_in_photos_callback) override;

 private:
  void IsFileArcWritableImpl(
      base::OnceCallback<void(bool)> is_file_arc_writable_callback,
      absl::optional<storage::FileSystemURL> url);
  void EditInPhotosImpl(const std::string& mime_type,
                        base::OnceCallback<void()> edit_in_photos_callback,
                        absl::optional<storage::FileSystemURL> url);

  content::WebUI* web_ui_;  // Owns |this|.

  scoped_refptr<ash::HatsNotificationController> hats_notification_controller_;

  base::WeakPtrFactory<ChromeMediaAppUIDelegate> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_MEDIA_APP_CHROME_MEDIA_APP_UI_DELEGATE_H_
