// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_DISABLED_AUTH_MESSAGE_VIEW_H_
#define ASH_LOGIN_UI_DISABLED_AUTH_MESSAGE_VIEW_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"

namespace ash {

struct AuthDisabledData;

// The message that can be shown to the user when auth is disabled.
class DisabledAuthMessageView : public views::View {
 public:
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(DisabledAuthMessageView* view);
    ~TestApi();

    const std::u16string& GetDisabledAuthMessageContent() const;

   private:
    const raw_ptr<DisabledAuthMessageView, ExperimentalAsh> view_;
  };

  DisabledAuthMessageView();

  DisabledAuthMessageView(const DisabledAuthMessageView&) = delete;
  DisabledAuthMessageView& operator=(const DisabledAuthMessageView&) = delete;

  ~DisabledAuthMessageView() override;

  // Set the parameters needed to render the message.
  void SetAuthDisabledMessage(const AuthDisabledData& auth_disabled_data,
                              bool use_24hour_clock);

  // Set the message title and content.
  void SetAuthDisabledMessage(const std::u16string& title,
                              const std::u16string& content);

  // views::View:
  void RequestFocus() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

 private:
  raw_ptr<views::Label, ExperimentalAsh> message_title_;
  raw_ptr<views::Label, ExperimentalAsh> message_contents_;
  raw_ptr<views::ImageView, ExperimentalAsh> message_icon_;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_DISABLED_AUTH_MESSAGE_VIEW_H_
