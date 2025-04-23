// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_IMAGE_CONTENT_ANNOTATOR_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_IMAGE_CONTENT_ANNOTATOR_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chromeos/services/machine_learning/public/mojom/image_content_annotation.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace base {
class FilePath;
}

namespace app_list {

class ImageContentAnnotator {
 public:
  using IcaDisconnectedCallback = base::RepeatingCallback<void()>;

  explicit ImageContentAnnotator(IcaDisconnectedCallback callback);
  ~ImageContentAnnotator();
  ImageContentAnnotator(const ImageContentAnnotator&) = delete;
  ImageContentAnnotator& operator=(const ImageContentAnnotator&) = delete;

  void EnsureAnnotatorIsConnected();
  void DisconnectAnnotator();
  void AnnotateEncodedImage(
      const base::FilePath& image_path,
      base::OnceCallback<
          void(chromeos::machine_learning::mojom::ImageAnnotationResultPtr)>
          callback);
  bool IsDlcInitialized() const;

  // TODO(b/306283895): This function is for debugging purpose only, and should
  // be removed when the ICA DLC bug has been fixed.
  void set_num_retries_passed(int num) { num_retries_passed_ = num; }

 private:
  void ConnectToImageAnnotator();

  void OnIcaDisconnected();

  mojo::Remote<chromeos::machine_learning::mojom::MachineLearningService>
      ml_service_;
  mojo::Remote<chromeos::machine_learning::mojom::ImageContentAnnotator>
      image_content_annotator_;

  bool ica_dlc_initialized_ = false;
  int num_retries_passed_ = 0;

  // Called when the `image_content_annotator_` is disconnected.
  IcaDisconnectedCallback ica_disconnected_callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ImageContentAnnotator> weak_ptr_factory_{this};
};
}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_IMAGE_CONTENT_ANNOTATOR_H_
