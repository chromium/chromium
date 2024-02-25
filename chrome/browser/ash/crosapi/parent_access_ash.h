// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_PARENT_ACCESS_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_PARENT_ACCESS_ASH_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_dialog.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_ui.mojom.h"
#include "chromeos/crosapi/mojom/parent_access.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

class GURL;

namespace gfx {
class ImageSkia;
}

namespace crosapi {

// Implements the crosapi ParentAccess interface. Lives in ash-chrome on
// the UI thread. Launches the ParentAccessDialog to get parent approval
// for an action by a child.
class ParentAccessAsh : public mojom::ParentAccess {
 public:
  ParentAccessAsh();
  ParentAccessAsh(const ParentAccessAsh&) = delete;
  ParentAccessAsh& operator=(const ParentAccessAsh&) = delete;
  ~ParentAccessAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::ParentAccess> receiver);

  // crosapi::mojom::ParentAccess:
  void GetWebsiteParentApproval(
      const GURL& url,
      const std::u16string& child_display_name,
      const gfx::ImageSkia& favicon,
      GetWebsiteParentApprovalCallback callback) override;

  void GetExtensionParentApproval(
      const std::u16string& extension_name,
      const std::u16string& child_display_name,
      const gfx::ImageSkia& icon,
      const std::vector<crosapi::mojom::ExtensionPermissionPtr> permissions,
      bool requests_disabled,
      GetExtensionParentApprovalCallback callback) override;

  using ParentAccessCallback =
      base::OnceCallback<void(crosapi::mojom::ParentAccessResultPtr)>;

  ash::ParentAccessDialogProvider* SetDialogProviderForTest(
      std::unique_ptr<ash::ParentAccessDialogProvider> provider);

 private:
  // Shows the dialog with the specified parameters.
  // params:  The parameters for the dialog.
  // callback:  The crosapi callback to call when dialog completes.
  void ShowParentAccessDialog(
      parent_access_ui::mojom::ParentAccessParamsPtr parent_access_params,
      ParentAccessCallback callback);

  // Common callback used when dialog closes.
  // callback:  The crosapi callback to call with the result.
  // result: The result returned from the dialog.
  void OnParentAccessDialogClosed(
      std::unique_ptr<ash::ParentAccessDialog::Result> result);

  // The returned object is lazy initialized.
  ash::ParentAccessDialogProvider* GetDialogProvider();

  mojo::ReceiverSet<mojom::ParentAccess> receivers_;
  std::unique_ptr<ash::ParentAccessDialogProvider> dialog_provider_;
  ParentAccessCallback callback_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_PARENT_ACCESS_ASH_H_
