// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/logo_view/logo_view.h"

#include "build/buildflag.h"
#include "chromeos/ash/components/assistant/buildflags.h"
#include "ui/base/metadata/metadata_impl_macros.h"

#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#include "ash/assistant/ui/logo_view/logo_view_impl.h"
#endif

namespace ash {

LogoView::LogoView() = default;

LogoView::~LogoView() = default;

// static
std::unique_ptr<LogoView> LogoView::Create() {
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
  return std::make_unique<LogoViewImpl>();
#else
  return std::make_unique<LogoView>();
#endif
}

BEGIN_METADATA(LogoView)
END_METADATA

}  // namespace ash
