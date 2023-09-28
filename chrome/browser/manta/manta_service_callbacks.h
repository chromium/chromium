// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANTA_MANTA_SERVICE_CALLBACKS_H_
#define CHROME_BROWSER_MANTA_MANTA_SERVICE_CALLBACKS_H_

#include <memory>

#include "base/functional/bind.h"
#include "chrome/browser/manta/manta_status.h"
#include "chrome/browser/manta/proto/manta.pb.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace manta {

// Manta service uses this callback to return a Response proto parsed
// from server response, and a MantaStatus struct that indicates OK status or
// errors if server does not respond properly.
using MantaProtoResponseCallback =
    base::OnceCallback<void(std::unique_ptr<manta::proto::Response>,
                            MantaStatus)>;

// Manta service uses this callback to return the parsed result / error messages
// to the caller.
using MantaGenericCallback =
    base::OnceCallback<void(base::Value::Dict, MantaStatus)>;

void OnEndpointFetcherComplete(MantaProtoResponseCallback callback,
                               std::unique_ptr<EndpointFetcher> fetcher,
                               std::unique_ptr<EndpointResponse> responses);

}  // namespace manta

#endif  // CHROME_BROWSER_MANTA_MANTA_SERVICE_CALLBACKS_H_
