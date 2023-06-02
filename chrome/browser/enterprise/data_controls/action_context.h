// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_ACTION_CONTEXT_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_ACTION_CONTEXT_H_

#include "build/chromeos_buildflags.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/enterprise/data_controls/component.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace data_controls {

// Generic struct that represents metadata about an action involved in Data
// Controls. It can be used to either represent a source or destination tied
// to an action.
struct ActionContext {
  GURL url;
#if BUILDFLAG(IS_CHROMEOS)
  Component component = Component::kUnknownComponent;
#endif  // BUILDFLAG(IS_CHROMEOS)
};

}  // namespace data_controls

#endif  // CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_ACTION_CONTEXT_H_
