// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/palette/tools/create_note_action.h"

#include "ash/public/cpp/note_taking_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/palette/palette_ids.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace {
NoteTakingClient* GetAvailableClient() {
  auto* client = NoteTakingClient::GetInstance();
  if (!client || !client->CanCreateNote())
    return nullptr;
  return client;
}
}  // namespace

CreateNoteAction::CreateNoteAction(Delegate* delegate)
    : CommonPaletteTool(delegate) {}

CreateNoteAction::~CreateNoteAction() = default;

PaletteGroup CreateNoteAction::GetGroup() const {
  return PaletteGroup::ACTION;
}

PaletteToolId CreateNoteAction::GetToolId() const {
  return PaletteToolId::CREATE_NOTE;
}

void CreateNoteAction::OnEnable() {
  CommonPaletteTool::OnEnable();

  auto* client = GetAvailableClient();
  if (client)
    client->CreateNote();

  delegate()->DisableTool(GetToolId());
  delegate()->HidePalette();
}

views::View* CreateNoteAction::CreateView() {
  if (!GetAvailableClient())
    return nullptr;

  return CreateDefaultView(
      l10n_util::GetStringUTF16(IDS_ASH_STYLUS_TOOLS_CREATE_NOTE_ACTION));
}

const gfx::VectorIcon& CreateNoteAction::GetPaletteIcon() const {
  return kPaletteActionCreateNoteIcon;
}

}  // namespace ash
