// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/glanceables_welcome_label.h"

#include <string>

#include "ash/public/cpp/session/session_types.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/check.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/views/controls/label.h"

namespace ash {

GlanceablesWelcomeLabel::GlanceablesWelcomeLabel() {
  SetAutoColorReadabilityEnabled(false);
  // TODO(crbug.com/1353488): Make font size customizable.
  SetFontList(gfx::FontList({"Google Sans"}, gfx::Font::FontStyle::NORMAL, 28,
                            gfx::Font::Weight::MEDIUM));
  SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  SetMultiLine(false);
  // TODO(crbug.com/1353488): Change to dynamic greeting based on system time.
  SetText(l10n_util::GetStringFUTF16(IDS_GLANCEABLES_WELCOME_LABEL,
                                     GetUserGivenName()));
}

void GlanceablesWelcomeLabel::OnThemeChanged() {
  views::Label::OnThemeChanged();
  // TODO(crbug.com/1353488): Use color provider.
  SetEnabledColor(gfx::kGoogleGrey200);
}

std::u16string GlanceablesWelcomeLabel::GetUserGivenName() const {
  DCHECK(Shell::Get());

  const auto* session_controller = Shell::Get()->session_controller();
  DCHECK(session_controller);

  const auto account_id = session_controller->GetActiveAccountId();
  if (account_id.empty()) {
    // Prevents failures in `GlanceablesTest`.
    // TODO(crbug.com/1353119): Remove this after switching to `AshTestBase`.
    return u"";
  }

  const auto* user_session =
      session_controller->GetUserSessionByAccountId(account_id);
  DCHECK(user_session);

  return base::UTF8ToUTF16(user_session->user_info.given_name);
}

BEGIN_METADATA(GlanceablesWelcomeLabel, views::Label)
END_METADATA

}  // namespace ash
