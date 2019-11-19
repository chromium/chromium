// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LANGUAGE_TRANSLATE_FRAME_BINDER_H_
#define CHROME_BROWSER_LANGUAGE_TRANSLATE_FRAME_BINDER_H_

#include "components/translate/content/common/translate.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class RenderFrameHost;
}

namespace language {

void BindContentTranslateDriver(
    mojo::PendingReceiver<translate::mojom::ContentTranslateDriver> receiver,
    content::RenderFrameHost* render_frame_host);
}

#endif  // CHROME_BROWSER_LANGUAGE_TRANSLATE_FRAME_BINDER_H_
