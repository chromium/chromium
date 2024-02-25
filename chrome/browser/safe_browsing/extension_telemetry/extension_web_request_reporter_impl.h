// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_EXTENSION_WEB_REQUEST_REPORTER_IMPL_H_
#define CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_EXTENSION_WEB_REQUEST_REPORTER_IMPL_H_

#include "base/supports_user_data.h"
#include "chrome/browser/profiles/profile.h"
#include "components/safe_browsing/content/common/safe_browsing.mojom.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace safe_browsing {

// Implements Mojo interface for renderers to collect telemetry from extensions
// and send to the Extension Telemetry Service in the browser process.
// A MojoSafeBrowsingImpl instance is destructed when the Mojo message pipe is
// disconnected or |user_data_| is destructed.
class ExtensionWebRequestReporterImpl
    : public mojom::ExtensionWebRequestReporter,
      public base::SupportsUserData::Data {
 public:
  explicit ExtensionWebRequestReporterImpl(Profile* profile);

  ExtensionWebRequestReporterImpl(const ExtensionWebRequestReporterImpl&) =
      delete;
  ExtensionWebRequestReporterImpl& operator=(
      const ExtensionWebRequestReporterImpl&) = delete;

  ~ExtensionWebRequestReporterImpl() override;

  static void Create(
      content::RenderProcessHost* render_process_host,
      mojo::PendingReceiver<mojom::ExtensionWebRequestReporter> receiver);

 private:
  // Needed since there's a Clone method in two parent classes.
  using base::SupportsUserData::Data::Clone;

  // mojom::ExtensionWebRequestReporter implementation.
  // |telemetry_url| is untrustworthy and should only be used for telemetry
  // purposes.
  void SendWebRequestData(
      const std::string& origin_extension_id,
      const GURL& telemetry_url,
      mojom::WebRequestProtocolType protocol_type,
      mojom::WebRequestContactInitiatorType contact_initiator_type) override;
  void Clone(mojo::PendingReceiver<mojom::ExtensionWebRequestReporter> receiver)
      override;

  mojo::ReceiverSet<mojom::ExtensionWebRequestReporter> receivers_;

  void OnMojoDisconnect();

  // Guaranteed to outlive this object since ExtensionWebRequestReporterImpl's
  // lifetime is tied to |user_data_|.
  raw_ptr<Profile> profile_;

  static const int kUserDataKey = 0;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_EXTENSION_WEB_REQUEST_REPORTER_IMPL_H_
