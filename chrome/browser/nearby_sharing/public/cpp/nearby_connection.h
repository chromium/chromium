// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_PUBLIC_CPP_NEARBY_CONNECTION_H_
#define CHROME_BROWSER_NEARBY_SHARING_PUBLIC_CPP_NEARBY_CONNECTION_H_

#include <stdint.h>
#include <vector>

#include "base/callback.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// A socket-like wrapper around Nearby Connections that allows for asynchronous
// reads and writes.
class NearbyConnection {
 public:
  using ReadCallback =
      base::OnceCallback<void(absl::optional<std::vector<uint8_t>> bytes)>;

  virtual ~NearbyConnection() = default;

  // Reads a stream of bytes from the remote device. Invoke |callback| when
  // there is incoming data or when the socket is closed. Previously set
  // callback will be replaced by |callback|. Must not be used on a already
  // closed connection.
  virtual void Read(ReadCallback callback) = 0;

  // Writes an outgoing stream of bytes to the remote device asynchronously.
  // Must not be used on a already closed connection.
  virtual void Write(std::vector<uint8_t> bytes) = 0;

  // Closes the socket and disconnects from the remote device. This object will
  // be invalidated after |callback| in SetDisconnectionListener is invoked.
  virtual void Close() = 0;

  // Listens to the socket being closed. Invoke |callback| when the socket is
  // closed. This object will be invalidated after |listener| is invoked.
  // Previously set listener will be replaced by |listener|.
  virtual void SetDisconnectionListener(base::OnceClosure listener) = 0;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_PUBLIC_CPP_NEARBY_CONNECTION_H_
