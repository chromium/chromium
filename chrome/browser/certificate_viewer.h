// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CERTIFICATE_VIEWER_H_
#define CHROME_BROWSER_CERTIFICATE_VIEWER_H_

#include "ui/gfx/native_ui_types.h"

namespace content {
class WebContents;
}

namespace net {
class X509Certificate;
}

// The UI surface from which the certificate viewer was opened.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(CertificateViewerEntrypoint)
enum class CertificateViewerEntrypoint {
  // The connection security subpage of the Page Info bubble. Shows the server
  // certificate chain for the current page.
  kPageInfo = 0,
  // The "view certificate" link in the details section of an SSL error
  // interstitial. Shows the server certificate chain that failed verification.
  kSslInterstitial = 1,
  // A certificate link in the DevTools Security panel. Shows a certificate
  // chain for the inspected page, supplied by the DevTools frontend.
  kDevTools = 2,
  // The view button on a certificate entry in the Certificate Manager
  // (chrome://certificate-manager). Shows a single certificate, optionally
  // with its Chrome-managed trust metadata.
  kCertificateManager = 3,
  // The client certificate selector dialog shown when a site requests TLS
  // client authentication. Shows a candidate client certificate. On Windows
  // and macOS this opens the native OS certificate viewer instead, and is not
  // recorded.
  kClientCertSelector = 4,
  kMaxValue = kClientCertSelector,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/net/enums.xml:CertificateViewerEntrypoint)

// Opens a certificate viewer under `parent` to display `cert`. `entrypoint`
// identifies the UI surface the viewer was opened from, for metrics.
void ShowCertificateViewer(content::WebContents* web_contents,
                           gfx::NativeWindow parent,
                           net::X509Certificate* cert,
                           CertificateViewerEntrypoint entrypoint);

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
