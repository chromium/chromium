// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_CONTEXT_MENU_MANAGER_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_CONTEXT_MENU_MANAGER_H_

#include "base/types/strong_alias.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace autofill {

// `AutofillContextMenuManager` is responsible for adding/executing Autofill
// related context menu items. `RenderViewContextMenu` is intended to own and
// control the lifetime of `AutofillContextMenuManager`.
class AutofillContextMenuManager {
 public:
  // Represents command id used to denote a row in the context menu. The
  // command ids are created when the items are added to the context menu during
  // it's initialization. For Autofill, it ranges from
  // `IDC_CONTENT_CONTEXT_AUTOFILL_CUSTOM_FIRST` to
  // `IDC_CONTENT_CONTEXT_AUTOFILL_CUSTOM_LAST`.
  using CommandId = base::StrongAlias<class CommandIdTag, int>;

  // Returns true if the given id is one generated for autofill context menu.
  static bool IsAutofillCustomCommandId(CommandId command_id);

  AutofillContextMenuManager() = default;
  AutofillContextMenuManager(const AutofillContextMenuManager&) = delete;
  AutofillContextMenuManager& operator=(const AutofillContextMenuManager&) =
      delete;

  // Adds items such as "Addresses"/"Credit Cards"/"Passwords" to the top level
  // of the context menu.
  void AppendTopLevelItems();

  // |AutofillContextMenuManager| specific, called from |RenderViewContextMenu|.
  bool IsCommandIdChecked(CommandId command_id) const;
  bool IsCommandIdVisible(CommandId command_id) const;
  bool IsCommandIdEnabled(CommandId command_id) const;
  void ExecuteCommand(CommandId command_id,
                      content::WebContents* web_contents,
                      content::RenderFrameHost* render_frame_host);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_CONTEXT_MENU_MANAGER_H_
