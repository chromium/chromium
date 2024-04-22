// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_NETWORK_ANNOTATION_MONITOR_H_
#define CHROME_BROWSER_NET_NETWORK_ANNOTATION_MONITOR_H_

#include "base/containers/flat_set.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/mojom/network_annotation_monitor.mojom.h"

// NetworkAnnotationMonitor monitors network calls reported via the `Report`
// method. Network calls are identified by their Network Annotation hash_code.
// It reads from profile prefs containing a set of network annotations that
// are expected to be disabled based on policy values. When a network annotation
// that matches an expected disabled annotation is reported, then an UMA metric
// is emitted for that hash_code.
class NetworkAnnotationMonitor
    : public network::mojom::NetworkAnnotationMonitor {
 public:
  NetworkAnnotationMonitor();
  NetworkAnnotationMonitor(const NetworkAnnotationMonitor&) = delete;
  NetworkAnnotationMonitor& operator=(const NetworkAnnotationMonitor&) = delete;
  ~NetworkAnnotationMonitor() override;

  mojo::PendingRemote<network::mojom::NetworkAnnotationMonitor> GetClient();

  void FlushForTesting();

 private:
  void Report(int32_t hash_code) override;

  mojo::Receiver<network::mojom::NetworkAnnotationMonitor> receiver_{this};
};

#endif  // CHROME_BROWSER_NET_NETWORK_ANNOTATION_MONITOR_H_
