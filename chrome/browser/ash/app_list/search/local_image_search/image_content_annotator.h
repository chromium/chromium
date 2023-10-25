// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_IMAGE_CONTENT_ANNOTATOR_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_IMAGE_CONTENT_ANNOTATOR_H_

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
  ImageContentAnnotator();
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

 private:
  void ConnectToImageAnnotator();

  mojo::Remote<chromeos::machine_learning::mojom::MachineLearningService>
      ml_service_;
  mojo::Remote<chromeos::machine_learning::mojom::ImageContentAnnotator>
      image_content_annotator_;

  bool ica_dlc_initialized_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
};
}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_LOCAL_IMAGE_SEARCH_IMAGE_CONTENT_ANNOTATOR_H_
