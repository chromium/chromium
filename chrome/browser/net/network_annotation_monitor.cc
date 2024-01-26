// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/network_annotation_monitor.h"

#include <utility>

#include "base/metrics/histogram_functions.h"

NetworkAnnotationMonitor::NetworkAnnotationMonitor() {
  // Add some example hard-coded annotations, for now. Later this list will be
  // generated dynamically based on policy values.
  disabled_annotations_.insert(88863520);  // autofill_query
}

NetworkAnnotationMonitor::~NetworkAnnotationMonitor() = default;

void NetworkAnnotationMonitor::Report(int32_t hash_code) {
  if (disabled_annotations_.contains(hash_code)) {
    base::UmaHistogramSparse("NetworkAnnotationMonitor.PolicyViolation",
                             hash_code);
  }
}

mojo::PendingRemote<network::mojom::NetworkAnnotationMonitor>
NetworkAnnotationMonitor::GetClient() {
  // Reset receiver if already bound. This can happen if the Network Service
  // crashed and has been restarted.
  if (receiver_.is_bound()) {
    receiver_.reset();
  }

  mojo::PendingRemote<network::mojom::NetworkAnnotationMonitor> client;
  receiver_.Bind(client.InitWithNewPipeAndPassReceiver());
  return client;
}

void NetworkAnnotationMonitor::SetDisabledAnnotationsForTesting(
    base::flat_set<int32_t> disabled_annotations) {
  disabled_annotations_ = std::move(disabled_annotations);
}

void NetworkAnnotationMonitor::FlushForTesting() {
  receiver_.FlushForTesting();  // IN-TEST
}
