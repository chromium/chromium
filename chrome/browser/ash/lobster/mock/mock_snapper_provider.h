// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOBSTER_MOCK_MOCK_SNAPPER_PROVIDER_H_
#define CHROME_BROWSER_ASH_LOBSTER_MOCK_MOCK_SNAPPER_PROVIDER_H_

#include "base/functional/callback_forward.h"
#include "components/manta/snapper_provider.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockSnapperProvider : virtual public manta::SnapperProvider {
 public:
  MockSnapperProvider();

  MockSnapperProvider(const MockSnapperProvider&) = delete;
  MockSnapperProvider& operator=(const MockSnapperProvider&) = delete;

  ~MockSnapperProvider() override;

  MOCK_METHOD(void,
              Call,
              (manta::proto::Request & request,
               net::NetworkTrafficAnnotationTag traffic_annotation,
               manta::MantaProtoResponseCallback done_callback),
              (override));
};

#endif  // CHROME_BROWSER_ASH_LOBSTER_MOCK_MOCK_SNAPPER_PROVIDER_H_
