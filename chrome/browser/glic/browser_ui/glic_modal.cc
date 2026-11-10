// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_modal.h"

#if !BUILDFLAG(IS_ANDROID)
#include "ui/views/view.h"
#endif

namespace glic {

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(GlicModal, kDialogElementId);

GlicModal::Params::Params() = default;
GlicModal::Params::Params(Params&&) = default;
GlicModal::Params& GlicModal::Params::operator=(Params&&) = default;
GlicModal::Params::~Params() = default;

#if BUILDFLAG(IS_ANDROID)
// TODO(b/477622144): Temporary stub until GlicModalAndroid lands in the
// follow-up CL.
// static
std::unique_ptr<GlicModal> GlicModal::Show(tabs::TabInterface& tab,
                                           Params params) {
  return nullptr;
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace glic
