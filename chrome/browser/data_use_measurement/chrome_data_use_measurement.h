// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DATA_USE_MEASUREMENT_CHROME_DATA_USE_MEASUREMENT_H_
#define CHROME_BROWSER_DATA_USE_MEASUREMENT_CHROME_DATA_USE_MEASUREMENT_H_

#include <cstdint>

#include "base/sequence_checker.h"

class ChromeDataUseMeasurement {
 public:
  static ChromeDataUseMeasurement& GetInstance();

  ChromeDataUseMeasurement(const ChromeDataUseMeasurement&) = delete;
  ChromeDataUseMeasurement& operator=(const ChromeDataUseMeasurement&) = delete;

  // Called when requests complete from NetworkService. Called for all requests
  // (including service requests and user-initiated requests).
  void ReportNetworkServiceDataUse(int32_t network_traffic_annotation_id_hash,
                                   int64_t recv_bytes,
                                   int64_t sent_bytes);

  ChromeDataUseMeasurement();
  ~ChromeDataUseMeasurement();

 private:
  // Specifies that data is received or sent, respectively.
  enum class TrafficDirection { kDownstream, kUpstream };

  // Records data use histograms. It gets the size of exchanged
  // message, its direction (which is upstream or downstream) and reports to the
  // histogram DataUse.Services.{Dimensions} with, services as the buckets.
  // |app_state| indicates the app state which can be foreground, or background.
  void ReportDataUsage(TrafficDirection dir, int64_t message_size_bytes);

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // CHROME_BROWSER_DATA_USE_MEASUREMENT_CHROME_DATA_USE_MEASUREMENT_H_
