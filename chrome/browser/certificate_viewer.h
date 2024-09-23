// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CERTIFICATE_VIEWER_H_
#define CHROME_BROWSER_CERTIFICATE_VIEWER_H_

#include "ui/gfx/native_widget_types.h"

namespace content {
class WebContents;
}

namespace net {
class X509Certificate;
}
// Opens a certificate viewer under |parent| to display |cert|.
void ShowCertificateViewer(content::WebContents* web_contents,
                           gfx::NativeWindow parent,
                           net::X509Certificate* cert);

// Opens a certificate viewer for client authentication under |parent| to
// display |cert|.
// TODO(crbug.com/40847472): remove this and use the internal cert
// viewer for client auth as well.
void ShowCertificateViewerForClientAuth(content::WebContents* web_contents,
                                        gfx::NativeWindow parent,
                                        net::X509Certificate* cert);

// Go through the motions but do not invoke the native API showing a modal
// interactive dialog on platforms where that results in hanging tests.
void MockCertificateViewerForTesting();

#endif  // CHROME_BROWSER_CERTIFICATE_VIEWER_H_
