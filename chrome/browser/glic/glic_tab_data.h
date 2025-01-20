// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_TAB_DATA_H_
#define CHROME_BROWSER_GLIC_GLIC_TAB_DATA_H_

#include "chrome/browser/glic/glic.mojom.h"
#include "content/public/browser/web_contents.h"

namespace glic {

// Populates and returns a TabDataPtr from a given WebContents, or null if
// web_contents is null.
glic::mojom::TabDataPtr CreateTabData(content::WebContents* web_contents);

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_TAB_DATA_H_
