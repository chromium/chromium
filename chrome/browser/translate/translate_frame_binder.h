// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_FRAME_BINDER_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_FRAME_BINDER_H_

#include "components/language_detection/content/common/language_detection.mojom-forward.h"
#include "components/translate/content/common/translate.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class RenderFrameHost;
}

namespace translate {

void BindContentTranslateDriver(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<translate::mojom::ContentTranslateDriver> receiver);
void BindContentLanguageDetectionDriver(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<
        language_detection::mojom::ContentLanguageDetectionDriver> receiver);
}

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_FRAME_BINDER_H_
