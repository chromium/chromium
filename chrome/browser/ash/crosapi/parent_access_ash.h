// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_PARENT_ACCESS_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_PARENT_ACCESS_ASH_H_

#include <string>

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

 private:
  mojo::ReceiverSet<mojom::ParentAccess> receivers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_PARENT_ACCESS_ASH_H_
