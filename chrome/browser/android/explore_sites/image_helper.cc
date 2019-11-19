// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/explore_sites/image_helper.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chrome/browser/android/explore_sites/explore_sites_types.h"
#include "services/data_decoder/public/cpp/decode_image.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/size.h"

namespace explore_sites {

namespace {

// Ratio of icon size to the amount of padding between the icons.
const int kIconPaddingScale = 8;

}  // namespace

// Class Job is used to manage multiple calls to the ImageHelper. Each request
// to the ImageHelper is handled by a single Job, which is then destroyed after
// it is finished.
class ImageHelper::Job {
 public:
  // WARNING: When ImageJobFinishedCallback is called, |this| may be deleted.
  // So nothing can be called after this callback.
  Job(ImageHelper* image_helper,
      ImageJobType job_type,
      ImageJobFinishedCallback job_finished_callback,
      BitmapCallback bitmap_callback,
      EncodedImageList images,
      int pixel_size);
  ~Job();

  // Start begins the work that a Job performs (decoding and composition).
  void Start();

  void DecodeImageBytes(std::unique_ptr<EncodedImageBytes> image_bytes);
  void OnDecodeSiteImageDone(const SkBitmap& decoded_image);
  void OnDecodeCategoryImageDone(const SkBitmap& decoded_image);
  std::unique_ptr<SkBitmap> CombineImages();

 private:
  ImageHelper* const image_helper_;
  const ImageJobType job_type_;
  ImageJobFinishedCallback job_finished_callback_;
  BitmapCallback bitmap_callback_;

  EncodedImageList images_;
  int num_icons_, pixel_size_;
  std::vector<SkBitmap> bitmaps_;

  base::WeakPtrFactory<Job> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(Job);
};

ImageHelper::Job::Job(ImageHelper* image_helper,
                      ImageJobType job_type,
                      ImageJobFinishedCallback job_finished_callback,
                      BitmapCallback bitmap_callback,
                      EncodedImageList images,
                      int pixel_size)
    : image_helper_(image_helper),
      job_type_(job_type),
      job_finished_callback_(std::move(job_finished_callback)),
      bitmap_callback_(std::move(bitmap_callback)),
      images_(std::move(images)),
      pixel_size_(pixel_size) {
  num_icons_ = (images_.size() < kFaviconsPerCategoryImage)
                   ? images_.size()
                   : kFaviconsPerCategoryImage;
}

ImageHelper::Job::~Job() = default;

void ImageHelper::Job::Start() {
  for (int i = 0; i < num_icons_; i++) {
    // TODO(freedjm): preserve order of images.
    DVLOG(1) << "Decoding image " << i + 1 << " of " << images_.size();
    DecodeImageBytes(std::move(images_[i]));
  }
}

void ImageHelper::Job::DecodeImageBytes(
    std::unique_ptr<EncodedImageBytes> image_bytes) {
  data_decoder::mojom::ImageDecoder::DecodeImageCallback callback;
  if (job_type_ == ImageJobType::kSiteIcon) {
    callback = base::BindOnce(&ImageHelper::Job::OnDecodeSiteImageDone,
                              weak_ptr_factory_.GetWeakPtr());
  } else {
    callback = base::BindOnce(&ImageHelper::Job::OnDecodeCategoryImageDone,
                              weak_ptr_factory_.GetWeakPtr());
  }

  data_decoder::DecodeImage(&image_helper_->data_decoder_, *image_bytes,
                            data_decoder::mojom::ImageCodec::DEFAULT, false,
                            data_decoder::kDefaultMaxSizeInBytes, gfx::Size(),
                            std::move(callback));
}

void RecordImageDecodedUMA(bool decoded) {
  UMA_HISTOGRAM_BOOLEAN("ExploreSites.ImageDecoded", decoded);
}

void ImageHelper::Job::OnDecodeSiteImageDone(const SkBitmap& decoded_image) {
  bool decode_success = !decoded_image.isNull();
  DVLOG(1) << "Decoded site image, result "
           << (decode_success ? "non-null" : "null");
  RecordImageDecodedUMA(decode_success);

  if (!decode_success) {
    std::move(bitmap_callback_).Run(nullptr);
  } else {
    std::move(bitmap_callback_).Run(std::make_unique<SkBitmap>(decoded_image));
  }
  std::move(job_finished_callback_).Run();
}

void ImageHelper::Job::OnDecodeCategoryImageDone(
    const SkBitmap& decoded_image) {
  bool decode_success = !decoded_image.isNull();
  DVLOG(1) << "Decoded image for category, result "
           << (decode_success ? "non-null" : "null");
  RecordImageDecodedUMA(decode_success);

  if (!decode_success) {
    num_icons_--;
  } else {
    bitmaps_.push_back(decoded_image);
  }

  if ((int)bitmaps_.size() == num_icons_) {  // On last image for category.
    std::unique_ptr<SkBitmap> category_bitmap = CombineImages();
    std::move(bitmap_callback_).Run(std::move(category_bitmap));
    std::move(job_finished_callback_).Run();
  }
}

SkBitmap ScaleBitmap(int icon_size, SkBitmap* bitmap) {
  DCHECK(bitmap);
  SkBitmap temp_bitmap;
  SkImageInfo scaledIconInfo = bitmap->info().makeWH(icon_size, icon_size);
  temp_bitmap.setInfo(scaledIconInfo);
  temp_bitmap.allocPixels();
  bool did_scale =
      bitmap->pixmap().scalePixels(temp_bitmap.pixmap(), kHigh_SkFilterQuality);
  if (!did_scale) {
    DLOG(ERROR) << "Unable to scale icon for category image.";
    return SkBitmap();
  }
  return temp_bitmap;
}

std::unique_ptr<SkBitmap> ImageHelper::Job::CombineImages() {
  DVLOG(1) << "num icons: " << num_icons_;
  if (num_icons_ == 0) {
    return nullptr;
  }

  DVLOG(1) << "pixel_size_: " << pixel_size_;
  int icon_padding_pixel_size = pixel_size_ / kIconPaddingScale;

  // Offset to write icons out of frame due to padding.
  int icon_write_offset = icon_padding_pixel_size / 2;

  SkBitmap composite_bitmap;
  SkImageInfo image_info = bitmaps_[0]
                               .info()
                               .makeWH(pixel_size_, pixel_size_)
                               .makeAlphaType(kPremul_SkAlphaType);

  composite_bitmap.setInfo(image_info);
  composite_bitmap.allocPixels();

  int icon_size = pixel_size_ / 2;

  // draw icons in correct areas
  switch (num_icons_) {
    case 1: {
      // Centered.
      SkBitmap scaledBitmap = ScaleBitmap(icon_size, &bitmaps_[0]);
      if (scaledBitmap.empty()) {
        return nullptr;
      }
      composite_bitmap.writePixels(
          scaledBitmap.pixmap(),
          ((icon_size + icon_padding_pixel_size) / 2) - icon_write_offset,
          ((icon_size + icon_padding_pixel_size) / 2) - icon_write_offset);
      break;
    }
    case 2: {
      // Side by side.
      for (int i = 0; i < 2; i++) {
        SkBitmap scaledBitmap = ScaleBitmap(icon_size, &bitmaps_[i]);
        if (scaledBitmap.empty()) {
          return nullptr;
        }
        composite_bitmap.writePixels(
            scaledBitmap.pixmap(),
            (i * (icon_size + icon_padding_pixel_size)) - icon_write_offset,
            ((icon_size + icon_padding_pixel_size) / 2) - icon_write_offset);
      }
      break;
    }
    case 3: {
      // Two on top, one on bottom.
      for (int i = 0; i < 3; i++) {
        SkBitmap scaledBitmap = ScaleBitmap(icon_size, &bitmaps_[i]);
        if (scaledBitmap.empty()) {
          return nullptr;
        }
        switch (i) {
          case 0:
            composite_bitmap.writePixels(
                scaledBitmap.pixmap(), -icon_write_offset, -icon_write_offset);
            break;
          case 1:
            composite_bitmap.writePixels(
                scaledBitmap.pixmap(),
                (icon_size + icon_padding_pixel_size) - icon_write_offset,
                -icon_write_offset);
            break;
          default:
            composite_bitmap.writePixels(
                scaledBitmap.pixmap(),
                ((icon_size + icon_padding_pixel_size) / 2) - icon_write_offset,
                (icon_size + icon_padding_pixel_size) - icon_write_offset);
            break;
        }
      }
      break;
    }
    case 4: {
      // One in each corner.
      for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
          int index = i + 2 * j;
          SkBitmap scaledBitmap = ScaleBitmap(icon_size, &bitmaps_[index]);
          if (scaledBitmap.empty()) {
            return nullptr;
          }
          composite_bitmap.writePixels(
              scaledBitmap.pixmap(),
              (j * (icon_size + icon_padding_pixel_size)) - icon_write_offset,
              (i * (icon_size + icon_padding_pixel_size)) - icon_write_offset);
        }
      }
      break;
    }
    default:
      DLOG(ERROR) << "Invalid number of icons to combine: " << bitmaps_.size();
      return nullptr;
  }

  return std::make_unique<SkBitmap>(composite_bitmap);
}

ImageHelper::ImageHelper() : last_used_job_id_(0) {}

ImageHelper::~ImageHelper() {}

void ImageHelper::NewJob(ImageJobType job_type,
                         ImageJobFinishedCallback job_finished_callback,
                         BitmapCallback bitmap_callback,
                         EncodedImageList images,
                         int pixel_size) {
  auto job = std::make_unique<Job>(
      this, job_type, std::move(job_finished_callback),
      std::move(bitmap_callback), std::move(images), pixel_size);
  id_to_job_[last_used_job_id_] = std::move(job);
  id_to_job_[last_used_job_id_]->Start();
}

void ImageHelper::OnJobFinished(int job_id) {
  DVLOG(1) << "Erasing job " << job_id;
  id_to_job_.erase(job_id);
}

void ImageHelper::ComposeSiteImage(BitmapCallback callback,
                                   EncodedImageList images) {
  DVLOG(1) << "Requested decoding for site image";
  if (images.size() == 0) {
    std::move(callback).Run(nullptr);
    return;
  }

  NewJob(ImageJobType::kSiteIcon,
         base::BindOnce(&ImageHelper::OnJobFinished, weak_factory_.GetWeakPtr(),
                        ++last_used_job_id_),
         std::move(callback), std::move(images), -1);
}

void ImageHelper::ComposeCategoryImage(BitmapCallback callback,
                                       int pixel_size,
                                       EncodedImageList images) {
  DVLOG(1) << "Requested decoding " << images.size()
           << " images for category image";

  if (images.size() == 0) {
    std::move(callback).Run(nullptr);
    return;
  }

  NewJob(ImageJobType::kCategoryImage,
         base::BindOnce(&ImageHelper::OnJobFinished, weak_factory_.GetWeakPtr(),
                        ++last_used_job_id_),
         std::move(callback), std::move(images), pixel_size);
}

}  // namespace explore_sites
