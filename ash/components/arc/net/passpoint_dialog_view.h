// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_NET_PASSPOINT_DIALOG_VIEW_H_
#define ASH_COMPONENTS_ARC_NET_PASSPOINT_DIALOG_VIEW_H_

#include <memory>

#include "ash/components/arc/mojom/net.mojom.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "ui/views/layout/box_layout_view.h"

namespace aura {
class Window;
}  // namespace aura

namespace arc {

// Dialog to let user allow or disallow an app to provision Passpoint
// credentials.
class PasspointDialogView : public views::BoxLayoutView {
 public:
  // Callback to notify user's confirmation for allowing an app to install
  // Passpoint credentials. If user allow it, the callback is invoked with true
  // as its argument. Otherwise, with false.
  using PasspointDialogCallback =
      base::OnceCallback<void(mojom::PasspointApprovalResponsePtr)>;

  PasspointDialogView(base::StringPiece app_name,
                      PasspointDialogCallback callback);
  PasspointDialogView(const PasspointDialogView&) = delete;
  PasspointDialogView& operator=(const PasspointDialogView&) = delete;
  ~PasspointDialogView() override;

  // Shows confirmation dialog for asking user to approve the app to install
  // Passpoint credentials.
  static void Show(aura::Window* parent,
                   base::StringPiece app_name,
                   PasspointDialogCallback callback);

  // views::View:
  gfx::Size CalculatePreferredSize() const override;

 private:
  std::unique_ptr<views::View> MakeContentsView();
  std::unique_ptr<views::View> MakeButtonsView();

  void OnLearnMoreClicked();
  void OnButtonClicked(bool allow);

  PasspointDialogCallback callback_;

  base::WeakPtrFactory<PasspointDialogView> weak_factory_{this};
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_NET_PASSPOINT_DIALOG_VIEW_H_
