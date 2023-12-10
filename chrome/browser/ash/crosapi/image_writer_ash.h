// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_IMAGE_WRITER_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_IMAGE_WRITER_ASH_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "chrome/common/extensions/api/image_writer_private.h"
#include "chrome/common/ref_counted_util.h"
#include "chromeos/crosapi/mojom/image_writer.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"

class GURL;

namespace crosapi {

// Implements crosapi image writer interface which performs operations on
// removable storage devices; and forwards calls for dispatching extension
// events to the image writer client.
class ImageWriterAsh : public mojom::ImageWriter {
 public:
  ImageWriterAsh();
  ImageWriterAsh(const ImageWriterAsh&) = delete;
  ImageWriterAsh& operator=(const ImageWriterAsh&) = delete;
  ~ImageWriterAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::ImageWriter> pending_receiver);

  // crosapi::mojom::ImageWriter:
  void ListRemovableStorageDevices(
      ListRemovableStorageDevicesCallback callback) override;
  void DestroyPartitions(
      const std::string& storage_unit_id,
      mojo::PendingRemote<mojom::ImageWriterClient> remote_client,
      DestroyPartitionsCallback callback) override;
  void WriteFromUrl(const std::string& storage_unit_id,
                    const GURL& image_url,
                    const std::optional<std::string>& image_hash,
                    mojo::PendingRemote<mojom::ImageWriterClient> remote_client,
                    WriteFromUrlCallback callback) override;
  void WriteFromFile(
      const std::string& storage_unit_id,
      const base::FilePath& image_path,
      mojo::PendingRemote<mojom::ImageWriterClient> remote_client,
      WriteFromFileCallback callback) override;

  // Dispatches OnWriteProgress event to the remote image writer client
  // identified by |client_token_string|.
  void DispatchOnWriteProgressEvent(const std::string& client_token_string,
                                    crosapi::mojom::Stage stage,
                                    uint32_t percent_complete);

  // Dispatches OnWriteComplete event to the remote image writer client
  // identified by |client_token_string|.
  void DispatchOnWriteCompleteEvent(const std::string& client_token_string);

  // Dispatches OnWriteError event to the remote image writer client
  // identified by |client_token_string|.
  void DispatchOnWriteErrorEvent(const std::string& client_token_string,
                                 crosapi::mojom::Stage stage,
                                 uint32_t percent_complete,
                                 const std::string& error);

  // Returns true if |id| is a remote image writer client token string.
  bool IsRemoteClientToken(const std::string& id) const;

 private:
  using StorageDeviceList = RefCountedVector<
      extensions::api::image_writer_private::RemovableStorageDevice>;

  // Called when an ImageWriterClient disconnects.
  void OnImageWriterClientDisconnected(
      const base::UnguessableToken& remote_client_token);

  void OnDeviceListReady(ListRemovableStorageDevicesCallback callback,
                         scoped_refptr<StorageDeviceList> device_list);

  // Called after an operation has been handled by either posting it to
  // perform on the removable disk, or returning with |error| due to sanity
  // check failure.
  using OperationCallback =
      base::OnceCallback<void(const std::optional<std::string>&)>;
  void OnOperationCompleted(OperationCallback callback,
                            bool success,
                            const std ::string& error);

  void OnCancelWriteDone(bool success, const std::string& error);

  // This class supports any number of connections. This allows ImageWriter to
  // have multiple, potentially thread-affine, remotes.
  mojo::ReceiverSet<mojom::ImageWriter> receivers_;

  // Cache remote image writer clients by the string version of the remote
  // client token.
  std::map<std::string, mojo::Remote<mojom::ImageWriterClient>>
      remote_image_writer_clients_;

  base::WeakPtrFactory<ImageWriterAsh> weak_ptr_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_IMAGE_WRITER_ASH_H_
