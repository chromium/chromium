// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/camera/camera_effects_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/wallpaper/sea_pen_image.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/camera/autozoom_controller_impl.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/video_conference/bubble/bubble_view_ids.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"
#include "ash/system/video_conference/video_conference_tray.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "ash/system/video_conference/video_conference_utils.h"
#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom-shared.h"
#include "media/capture/video/chromeos/video_capture_features_chromeos.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_util.h"
#include "ui/gfx/vector_icon_types.h"

namespace ash {

namespace {

// A `std::pair` representation of the background blur state that
// `CameraHalDispatcherImpl` expects:
// - `BlurLevel` that specifies how much blur to apply
// - `bool` that's 'true' if background blur is enabled, false otherwise
using CameraHalBackgroundBlurState = std::pair<cros::mojom::BlurLevel, bool>;

using BackgroundImageInfo = CameraEffectsController::BackgroundImageInfo;

// Directory used for saving camera backgrounds.
constexpr char kCameraBackgroundOriginalDir[] =
    "custom-camera-backgrounds/original";

constexpr char kMetadataSuffix[] = ".metadata";

constexpr char kSupportedImages[] = FILE_PATH_LITERAL("*.jpg");

constexpr unsigned int k3M = 3 * 1024 * 1024;

// Max number of images kept as camera background.
constexpr unsigned int kMaxNumberOfImageKeptOnDisk = 12;

// Directory that can be accessed by the camera module.
constexpr char kImageDirForCameraModule[] = "/run/camera/";

// Returns 'true' if `pref_value` is an allowable value of
// `CameraEffectsController::BackgroundBlurPrefValue`, 'false' otherwise.
bool IsValidBackgroundBlurPrefValue(int pref_value) {
  switch (pref_value) {
    case CameraEffectsController::BackgroundBlurPrefValue::kOff:
    case CameraEffectsController::BackgroundBlurPrefValue::kLowest:
    case CameraEffectsController::BackgroundBlurPrefValue::kLight:
    case CameraEffectsController::BackgroundBlurPrefValue::kMedium:
    case CameraEffectsController::BackgroundBlurPrefValue::kHeavy:
    case CameraEffectsController::BackgroundBlurPrefValue::kMaximum:
    case CameraEffectsController::BackgroundBlurPrefValue::kImage:
      return true;
  }

  return false;
}

// Maps `pref_value` (assumed to be a value read out of
// `prefs::kBackgroundBlur`) to a `CameraHalBackgroundBlurState` (that
// `CameraHalDispatcherImpl` expects).
CameraHalBackgroundBlurState MapBackgroundBlurPrefValueToCameraHalState(
    int pref_value) {
  DCHECK(IsValidBackgroundBlurPrefValue(pref_value));

  switch (pref_value) {
    // For state `kOff`, the `bool` is 'false' because background blur is
    // disabled, `BlurLevel` is set to `kLowest` but its value doesn't matter.
    case CameraEffectsController::BackgroundBlurPrefValue::kOff:
    case CameraEffectsController::BackgroundBlurPrefValue::kImage:
      return std::make_pair(cros::mojom::BlurLevel::kLowest, false);

    // For states other than `kOff`, background blur is enabled so the `bool`
    // is set to 'true' and `pref_value` is mapped to a `BlurLevel`.
    case CameraEffectsController::BackgroundBlurPrefValue::kLowest:
      return std::make_pair(cros::mojom::BlurLevel::kLowest, true);
    case CameraEffectsController::BackgroundBlurPrefValue::kLight:
      return std::make_pair(cros::mojom::BlurLevel::kLight, true);
    case CameraEffectsController::BackgroundBlurPrefValue::kMedium:
      return std::make_pair(cros::mojom::BlurLevel::kMedium, true);
    case CameraEffectsController::BackgroundBlurPrefValue::kHeavy:
      return std::make_pair(cros::mojom::BlurLevel::kHeavy, true);
    case CameraEffectsController::BackgroundBlurPrefValue::kMaximum:
      return std::make_pair(cros::mojom::BlurLevel::kMaximum, true);
  }

  NOTREACHED();
}

// Maps the `CameraHalDispatcherImpl`-ready background blur state
// `level`/`enabled` to `CameraEffectsController::BackgroundBlurPrefValue`,
// which is what's written to `prefs::kBackgroundBlur`.
CameraEffectsController::BackgroundBlurPrefValue
MapBackgroundBlurCameraHalStateToPrefValue(cros::mojom::BlurLevel level,
                                           bool enabled) {
  if (!enabled) {
    return CameraEffectsController::BackgroundBlurPrefValue::kOff;
  }

  switch (level) {
    case cros::mojom::BlurLevel::kLowest:
      return CameraEffectsController::BackgroundBlurPrefValue::kLowest;
    case cros::mojom::BlurLevel::kLight:
      return CameraEffectsController::BackgroundBlurPrefValue::kLight;
    case cros::mojom::BlurLevel::kMedium:
      return CameraEffectsController::BackgroundBlurPrefValue::kMedium;
    case cros::mojom::BlurLevel::kHeavy:
      return CameraEffectsController::BackgroundBlurPrefValue::kHeavy;
    case cros::mojom::BlurLevel::kMaximum:
      return CameraEffectsController::BackgroundBlurPrefValue::kMaximum;
  }

  NOTREACHED();
}

CameraEffectsController::BackgroundBlurState MapBackgroundBlurPrefValueToState(
    int pref_value) {
  DCHECK(IsValidBackgroundBlurPrefValue(pref_value));

  switch (pref_value) {
    case CameraEffectsController::BackgroundBlurPrefValue::kOff:
      return CameraEffectsController::BackgroundBlurState::kOff;
    case CameraEffectsController::BackgroundBlurPrefValue::kLowest:
      return CameraEffectsController::BackgroundBlurState::kLowest;
    case CameraEffectsController::BackgroundBlurPrefValue::kLight:
      return CameraEffectsController::BackgroundBlurState::kLight;
    case CameraEffectsController::BackgroundBlurPrefValue::kMedium:
      return CameraEffectsController::BackgroundBlurState::kMedium;
    case CameraEffectsController::BackgroundBlurPrefValue::kHeavy:
      return CameraEffectsController::BackgroundBlurState::kHeavy;
    case CameraEffectsController::BackgroundBlurPrefValue::kMaximum:
      return CameraEffectsController::BackgroundBlurState::kMaximum;
    case CameraEffectsController::BackgroundBlurPrefValue::kImage:
      return CameraEffectsController::BackgroundBlurState::kImage;
  }

  NOTREACHED();
}

inline base::FilePath GetMetadataFilePath(const base::FilePath& filepath) {
  return filepath.AddExtensionASCII(kMetadataSuffix);
}

// Remove the file and its metadata if exists.
bool RemoveBackgroundImageOnWorker(const base::FilePath& filepath) {
  if (!base::DeleteFile(filepath)) {
    return false;
  }

  const auto metadata_filepath = GetMetadataFilePath(filepath);
  if (base::PathExists(metadata_filepath) &&
      !base::DeleteFile(metadata_filepath)) {
    return false;
  }

  return true;
}

// Writes `jpeg_bytes` to the `camera_background_img_dir`.
// Returns basename if succeeds, empty path otherwise.
base::FilePath WriteImageToBackgroundDir(
    const base::FilePath& camera_background_img_dir,
    SeaPenImage&& sea_pen_image,
    const std::string& metadata) {
  const base::FilePath basename =
      CameraEffectsController::SeaPenIdToRelativePath(sea_pen_image.id);
  const base::FilePath background_image_filepath =
      camera_background_img_dir.Append(basename);
  const base::FilePath background_metadata_filepath =
      GetMetadataFilePath(background_image_filepath);

  if (base::CreateDirectory(camera_background_img_dir) &&
      base::WriteFile(background_image_filepath, sea_pen_image.jpg_bytes) &&
      base::WriteFile(background_metadata_filepath, metadata)) {
    return basename;
  }

  // We don't want keep corrupted images.
  RemoveBackgroundImageOnWorker(background_image_filepath);
  return base::FilePath();
}

// Copies image file from `background_image_filepath` to
// `background_run_filepath`.
bool CopyBackgroundImageFile(const base::FilePath& background_image_filepath,
                             const base::FilePath& background_run_filepath) {
  const base::FilePath background_run_dir = background_run_filepath.DirName();
  const base::FilePath basename = background_run_filepath.BaseName();

  if (base::CreateDirectory(background_run_dir) &&
      base::CopyFile(background_image_filepath, background_run_filepath)) {
    base::File::Info file_info;
    base::GetFileInfo(background_image_filepath, &file_info);
    base::TouchFile(background_image_filepath, base::Time::Now(),
                    file_info.last_modified);

    // Remove all other images in the background_run_dir`.
    base::FileEnumerator enumerator(
        background_run_dir,
        /*recursive=*/false, base::FileEnumerator::FILES, kSupportedImages);
    for (auto path = enumerator.Next(); !path.empty();
         path = enumerator.Next()) {
      if (enumerator.GetInfo().GetName() != basename) {
        base::DeleteFile(path);
      }
    }

    return true;
  }
  LOG(ERROR) << "Can't copy " << background_image_filepath << " to "
             << background_run_filepath;

  return false;
}

// Returns a full list of files inside `camera_background_img_dir`, sorted by
// last_accessed time.
std::vector<base::FilePath> GetBackgroundImageFileNamesOnWorker(
    const base::FilePath& camera_background_img_dir) {
  using FileNameAndTime = std::pair<base::FilePath, base::Time>;

  std::vector<FileNameAndTime> filenames_and_modified_time;

  // Loop through all files in `camera_background_img_dir`.
  base::FileEnumerator enumerator(
      camera_background_img_dir,
      /*recursive=*/false, base::FileEnumerator::FILES, kSupportedImages);
  for (auto path = enumerator.Next(); !path.empty(); path = enumerator.Next()) {
    base::File::Info file_info;
    base::GetFileInfo(path, &file_info);
    filenames_and_modified_time.push_back(
        {path.BaseName(), file_info.last_accessed});
  }

  // Sorted by last_accessed.
  std::sort(filenames_and_modified_time.begin(),
            filenames_and_modified_time.end(),
            [](const FileNameAndTime& f1, const FileNameAndTime& f2) {
              return f1.second > f2.second;
            });

  // Only keep the latest `kMaxNumberOfImageKeptOnDisk` images on disk.
  if (filenames_and_modified_time.size() > kMaxNumberOfImageKeptOnDisk) {
    for (std::size_t i = kMaxNumberOfImageKeptOnDisk;
         i < filenames_and_modified_time.size(); i++) {
      const auto filename = camera_background_img_dir.Append(
          filenames_and_modified_time[i].first);
      RemoveBackgroundImageOnWorker(filename);
    }

    filenames_and_modified_time.resize(kMaxNumberOfImageKeptOnDisk);
  }

  std::vector<base::FilePath> filenames;
  for (auto& filename_and_time : filenames_and_modified_time) {
    filenames.push_back(filename_and_time.first);
  }

  return filenames;
}

// Gets the BackgroundImageInfo of the `filename`.
std::optional<BackgroundImageInfo> GetBackgroundImageInfoOnWorker(
    const base::FilePath& filename) {
  base::File::Info file_info;
  if (!base::GetFileInfo(filename, &file_info)) {
    return std::nullopt;
  }

  BackgroundImageInfo info{file_info.creation_time, file_info.last_accessed,
                           filename.BaseName(), gfx::ImageSkia(), ""};

  const std::optional<std::vector<uint8_t>> jpeg_bytes =
      base::ReadFileToBytes(filename);
  if (!jpeg_bytes) {
    return std::nullopt;
  }

  auto image = gfx::ImageFrom1xJPEGEncodedData(jpeg_bytes.value());
  if (image.IsEmpty()) {
    return std::nullopt;
  }

  if (image.Width() > CameraEffectsController::kImageAsIconWidth) {
    const auto new_size = gfx::ScaleToCeiledSize(
        image.Size(),
        static_cast<float>(CameraEffectsController::kImageAsIconWidth) /
            image.Width());
    image = gfx::ResizedImage(image, new_size);
  }
  info.image = image.AsImageSkia();

  // if the metadata is not read successfully, then set it as empty.
  if (!base::ReadFileToString(GetMetadataFilePath(filename), &info.metadata)) {
    info.metadata = "";
  }

  return info;
}

// Reads from the `camera_background_img_dir` for the BackgroundImageInfo of the
// latest `number_of_images`.
std::vector<BackgroundImageInfo> GetRecentlyUsedBackgroundImagesOnWorker(
    const std::size_t number_of_images,
    const base::FilePath& camera_background_img_dir) {
  std::vector<base::FilePath> basenames =
      GetBackgroundImageFileNamesOnWorker(camera_background_img_dir);

  std::vector<BackgroundImageInfo> background_image_info;

  // Adds creation_time and jpeg_bytes for each image file.
  for (auto& basename : basenames) {
    const auto info = GetBackgroundImageInfoOnWorker(
        camera_background_img_dir.Append(basename));

    if (!info.has_value()) {
      continue;
    }

    background_image_info.push_back(info.value());

    if (background_image_info.size() == number_of_images) {
      break;
    }
  }

  return background_image_info;
}

void SetBackgroundReplaceUiVisible(bool visible) {
  for (auto* root_window_controller :
       Shell::Get()->GetAllRootWindowControllers()) {
    DCHECK(root_window_controller);
    DCHECK(root_window_controller->GetStatusAreaWidget());

    root_window_controller->GetStatusAreaWidget()
        ->video_conference_tray()
        ->SetBackgroundReplaceUiVisible(visible);
  }
}

cros::mojom::InferenceBackend GetInferenceBackend(
    const base::Feature& feature) {
  const std::string value =
      GetFieldTrialParamValueByFeature(feature, "inference_backend");
  if (value == "AUTO") {
    return cros::mojom::InferenceBackend::kAuto;
  } else if (value == "GPU") {
    return cros::mojom::InferenceBackend::kGpu;
  } else if (value == "NPU") {
    return cros::mojom::InferenceBackend::kNpu;
  } else {
    // If the feature is disabled, or enabled without a specific value, we will
    // get an empty string and fall into this case.
    return cros::mojom::InferenceBackend::kDefaultValue;
  }
}

}  // namespace

// static
void CameraEffectsController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  if (!features::IsVideoConferenceEnabled()) {
    return;
  }

  // We have to register all camera effects prefs; because we need use them to
  // construct the cros::mojom::EffectsConfigPtr.
  registry->RegisterIntegerPref(prefs::kBackgroundBlur,
                                BackgroundBlurPrefValue::kOff);

  registry->RegisterBooleanPref(prefs::kBackgroundReplace, false);

  registry->RegisterBooleanPref(prefs::kPortraitRelighting, false);
  registry->RegisterBooleanPref(prefs::kFaceRetouch, false);

  // If the Studio Look feature is available, disable Studio Look by default.
  // Otherwise, set it to always true to apply effects based on the portrait
  // relighting and face retouch pref values.
  registry->RegisterBooleanPref(prefs::kStudioLook,
                                !features::IsVcStudioLookEnabled());

  registry->RegisterFilePathPref(prefs::kBackgroundImagePath, base::FilePath());
}

// static
base::FilePath CameraEffectsController::SeaPenIdToRelativePath(uint32_t id) {
  return base::FilePath(base::NumberToString(id)).AddExtension(".jpg");
}

BackgroundImageInfo::BackgroundImageInfo(const BackgroundImageInfo& info) =
    default;
BackgroundImageInfo::BackgroundImageInfo(const base::Time& creation_time,
                                         const base::Time& last_accessed,
                                         const base::FilePath& basename,
                                         const gfx::ImageSkia& image,
                                         const std::string& metadata)
    : creation_time(creation_time),
      last_accessed(last_accessed),
      basename(basename),
      image(image),
      metadata(metadata) {}

CameraEffectsController::CameraEffectsController()
    : camera_background_run_dir_(kImageDirForCameraModule),
      main_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      blocking_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
  auto* session_controller = Shell::Get()->session_controller();
  DCHECK(session_controller);
  session_observation_.Observe(session_controller);

  current_effects_ = cros::mojom::EffectsConfig::New();

  // The effects are not applied when this is constructed, observe for changes
  // that will come later.
  scoped_camera_effect_observation_.Observe(
      media::CameraHalDispatcherImpl::GetInstance());

  Shell::Get()->autozoom_controller()->AddObserver(this);

  VideoConferenceTrayController::Get()->GetEffectsManager().AddObserver(this);
}

CameraEffectsController::~CameraEffectsController() {
  VideoConferenceTrayEffectsManager& effects_manager =
      VideoConferenceTrayController::Get()->GetEffectsManager();
  if (effects_manager.IsDelegateRegistered(this)) {
    // The `VcEffectsDelegate` was registered, so must therefore be
    // unregistered.
    effects_manager.UnregisterDelegate(this);
  }

  Shell::Get()->autozoom_controller()->RemoveObserver(this);
}

cros::mojom::EffectsConfigPtr CameraEffectsController::GetCameraEffects() {
  return current_effects_.Clone();
}

void CameraEffectsController::SetBackgroundImage(
    const base::FilePath& relative_path,
    base::OnceCallback<void(bool)> callback) {
  CHECK(!camera_background_img_dir_.empty())
      << "SetBackgroundImage should not be called when "
         "camera_background_img_dir_ is not set.";

  cros::mojom::EffectsConfigPtr new_effects = current_effects_.Clone();

  if (new_effects->replace_enabled &&
      new_effects->background_filepath == relative_path) {
    std::move(callback).Run(true);
    return;
  }

  new_effects->replace_enabled = true;
  new_effects->background_filepath = relative_path;

  SetCameraEffects(std::move(new_effects), /*is_initialization*/ false,
                   std::move(callback));
}

void CameraEffectsController::SetBackgroundImageFromContent(
    const SeaPenImage& sea_pen_image,
    const std::string& metadata,
    base::OnceCallback<void(bool)> callback) {
  CHECK(!camera_background_img_dir_.empty())
      << "SetBackgroundImageFromContent should not be called when "
         "camera_background_img_dir_ is not set.";

  CHECK(!sea_pen_image.jpg_bytes.empty());
  CHECK_LT(sea_pen_image.jpg_bytes.size(), k3M)
      << "Can't use an image that is larger than 30M as a background";

  // Write images to disk;
  // TODO(b/321122378) remove unnecessary copy of SeaPenImage.
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&WriteImageToBackgroundDir, camera_background_img_dir_,
                     SeaPenImage(sea_pen_image.jpg_bytes, sea_pen_image.id),
                     metadata),
      base::BindOnce(
          &CameraEffectsController::OnSaveBackgroundImageFileComplete,
          weak_factory_.GetWeakPtr(), std::move(callback)));
}

void CameraEffectsController::RemoveBackgroundImage(
    const base::FilePath& basename,
    base::OnceCallback<void(bool)> callback) {
  CHECK(!camera_background_img_dir_.empty())
      << "RemoveBackgroundImage should not be called when "
         "camera_background_img_dir_ is not set.";

  // If the file to remove is current camera background, then reset the camera
  // background effects.
  if (basename == current_effects_->background_filepath) {
    cros::mojom::EffectsConfigPtr new_effects = GetCameraEffects();
    new_effects->replace_enabled = false;
    new_effects->background_filepath.reset();

    SetCameraEffects(std::move(new_effects), /*is_initialization*/ false,
                     base::NullCallback());
  }

  // Remove file.
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&RemoveBackgroundImageOnWorker,
                     camera_background_img_dir_.Append(basename)),
      std::move(callback));
}

void CameraEffectsController::GetRecentlyUsedBackgroundImages(
    const int number_of_images,
    base::OnceCallback<void(const std::vector<BackgroundImageInfo>&)>
        callback) {
  CHECK(!camera_background_img_dir_.empty())
      << "GetRecentlyUsedBackgroundImages should not be called when "
         "camera_background_img_dir_ is not set.";

  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&GetRecentlyUsedBackgroundImagesOnWorker, number_of_images,
                     camera_background_img_dir_),
      std::move(callback));
}

void CameraEffectsController::GetBackgroundImageFileNames(
    base::OnceCallback<void(const std::vector<base::FilePath>&)> callback) {
  CHECK(!camera_background_img_dir_.empty())
      << "GetBackgroundImageFileNames should not be called when "
         "camera_background_img_dir_ is not set.";

  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&GetBackgroundImageFileNamesOnWorker,
                     camera_background_img_dir_),
      std::move(callback));
}

void CameraEffectsController::GetBackgroundImageInfo(
    const base::FilePath& basename,
    base::OnceCallback<void(const std::optional<BackgroundImageInfo>&)>
        callback) {
  CHECK(!camera_background_img_dir_.empty())
      << "GetRecentlyUsedBackgroundImages should not be called when "
         "camera_background_img_dir_ is not set.";

  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&GetBackgroundImageInfoOnWorker,
                     camera_background_img_dir_.Append(basename)),
      std::move(callback));
}

bool CameraEffectsController::IsEligibleForBackgroundReplace() {
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  if (!session_controller) {
    return false;
  }

  AccountId account_id =
      Shell::Get()->session_controller()->GetActiveAccountId();
  return features::IsVcBackgroundReplaceEnabled() &&
         std::get<0>(Shell::Get()->session_controller()->IsEligibleForSeaPen(
             account_id));
}

bool CameraEffectsController::IsVcBackgroundAllowedByEnterprise() {
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  if (!session_controller) {
    return false;
  }

  AccountId account_id = session_controller->GetActiveAccountId();
  return std::get<1>(session_controller->IsEligibleForSeaPen(account_id));
}

// Set the `camera_background_img_dir_` when the `account_id` becomes active.
void CameraEffectsController::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  is_eligible_for_background_replace_ = IsEligibleForBackgroundReplace();

  is_background_replace_disabled_by_enterprise_ =
      !IsVcBackgroundAllowedByEnterprise();

  const base::FilePath profile_path =
      Shell::Get()->session_controller()->GetProfilePath(account_id);
  CHECK(!profile_path.empty())
      << "Profile path should not be empty in OnActiveUserSessionChanged.";

  camera_background_img_dir_ =
      profile_path.Append(kCameraBackgroundOriginalDir);

  // Initialze camera effects if the `pref_change_registrar_` is set.
  // TODO(b/321585013): figure out the order of OnActiveUserSessionChanged and
  // OnActiveUserPrefServiceChanged, and only initialize in one place.
  if (pref_change_registrar_) {
    SetCameraEffects(GetEffectsConfigFromPref(), /*is_initialization*/ true,
                     base::DoNothing());
  }

  // If any effects have controls the user can access, this will create the
  // effects UI and register `CameraEffectsController`'s `VcEffectsDelegate`
  // interface.
  InitializeEffectControls();
}

void CameraEffectsController::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  if (pref_change_registrar_ &&
      pref_service == pref_change_registrar_->prefs()) {
    return;
  }

  // Initial login and user switching in multi profiles.
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(pref_service);

  // Initialze camera effects if the `camera_background_img_dir_` is set.
  if (!camera_background_img_dir_.empty()) {
    // If the camera has started, it won't get the previous setting so call it
    // here too. If the camera service isn't ready it this call will be ignored.
    SetCameraEffects(GetEffectsConfigFromPref(), /*is_initialization*/ true,
                     base::DoNothing());
  }
}

std::optional<int> CameraEffectsController::GetEffectState(
    VcEffectId effect_id) {
  switch (effect_id) {
    case VcEffectId::kBackgroundBlur:
      return current_effects_->replace_enabled
                 ? CameraEffectsController::BackgroundBlurPrefValue::kImage
                 : MapBackgroundBlurCameraHalStateToPrefValue(
                       current_effects_->blur_level,
                       current_effects_->blur_enabled);
    case VcEffectId::kPortraitRelighting:
      return current_effects_->relight_enabled;
    case VcEffectId::kFaceRetouch:
      return current_effects_->retouch_enabled;
    case VcEffectId::kStudioLook:
      return current_effects_->studio_look_enabled;
    case VcEffectId::kCameraFraming:
      return Shell::Get()->autozoom_controller()->GetState() !=
             cros::mojom::CameraAutoFramingState::OFF;
    case VcEffectId::kNoiseCancellation:
    case VcEffectId::kStyleTransfer:
    case VcEffectId::kLiveCaption:
    case VcEffectId::kTestEffect:
      NOTREACHED();
  }
}

void CameraEffectsController::OnEffectControlActivated(
    VcEffectId effect_id,
    std::optional<int> state) {
  cros::mojom::EffectsConfigPtr new_effects = current_effects_.Clone();

  switch (effect_id) {
    case VcEffectId::kBackgroundBlur: {
      // UI should not pass in any invalid state.
      if (!state.has_value() ||
          !IsValidBackgroundBlurPrefValue(state.value())) {
        state = static_cast<int>(
            CameraEffectsController::BackgroundBlurPrefValue::kOff);
      }
      if (state.value() ==
          CameraEffectsController::BackgroundBlurPrefValue::kImage) {
        SetBackgroundReplaceUiVisible(true);

        // Clicking on the Image button should just show the
        // BackgroundReplaceUi, no effects change is required.
        return;
      }

      // Only change the SetCameraBackgroundView visibility if background
      // replace is enabled; otherwise the view is null.
      if (is_eligible_for_background_replace_) {
        SetBackgroundReplaceUiVisible(false);
      }

      auto [blur_level, blur_enabled] =
          MapBackgroundBlurPrefValueToCameraHalState(state.value());
      new_effects->blur_level = blur_level;
      new_effects->blur_enabled = blur_enabled;

      // No matter which background blur button the user clicked on, we should
      // always turn off background replace.
      new_effects->replace_enabled = false;
      new_effects->background_filepath.reset();
      break;
    }
    case VcEffectId::kPortraitRelighting: {
      new_effects->relight_enabled =
          state.value_or(!new_effects->relight_enabled);
      if (!features::IsVcStudioLookEnabled()) {
        // Make sure that `studio_look_enabled` is set to true. Otherwise, this
        // will override the value of `relight_enabled`.
        new_effects->studio_look_enabled = true;
      } else {
        new_effects->studio_look_enabled =
            new_effects->relight_enabled || new_effects->retouch_enabled;
      }
      break;
    }
    case VcEffectId::kFaceRetouch: {
      new_effects->retouch_enabled =
          state.value_or(!new_effects->retouch_enabled);
      if (!features::IsVcStudioLookEnabled()) {
        // Make sure that `studio_look_enabled` is set to true. Otherwise, this
        // will override the value of `retouch_enabled`.
        new_effects->studio_look_enabled = true;
      } else {
        new_effects->studio_look_enabled =
            new_effects->relight_enabled || new_effects->retouch_enabled;
      }
      break;
    }
    case VcEffectId::kStudioLook: {
      new_effects->studio_look_enabled =
          state.value_or(!new_effects->studio_look_enabled);
      new_effects->relight_enabled = new_effects->studio_look_enabled;
      new_effects->retouch_enabled = new_effects->studio_look_enabled;
      break;
    }
    case VcEffectId::kCameraFraming: {
      Shell::Get()->autozoom_controller()->Toggle();
      break;
    }
    case VcEffectId::kNoiseCancellation:
    case VcEffectId::kStyleTransfer:
    case VcEffectId::kLiveCaption:
    case VcEffectId::kTestEffect:
      NOTREACHED();
  }

  if (new_effects->studio_look_enabled !=
      current_effects_->studio_look_enabled) {
    VideoConferenceTrayController::Get()
        ->GetEffectsManager()
        .NotifyEffectChanged(VcEffectId::kStudioLook,
                             new_effects->studio_look_enabled);
  }

  SetCameraEffects(std::move(new_effects), /*is_initialization*/ false,
                   base::DoNothing());
}

void CameraEffectsController::RecordMetricsForSetValueEffectOnClick(
    VcEffectId effect_id,
    int state_value) const {
  // `CameraEffectsController` currently only has background blur as a set-value
  // effect, so it shouldn't be any other effects here.
  DCHECK_EQ(VcEffectId::kBackgroundBlur, effect_id);

  base::UmaHistogramEnumeration(
      video_conference_utils::GetEffectHistogramNameForClick(effect_id),
      MapBackgroundBlurPrefValueToState(state_value));
}

void CameraEffectsController::RecordMetricsForSetValueEffectOnStartup(
    VcEffectId effect_id,
    int state_value) const {
  // `CameraEffectsController` currently only has background blur as a set-value
  // effect, so it shouldn't be any other effects here.
  DCHECK_EQ(VcEffectId::kBackgroundBlur, effect_id);

  base::UmaHistogramEnumeration(
      video_conference_utils::GetEffectHistogramNameForInitialState(effect_id),
      MapBackgroundBlurPrefValueToState(state_value));
}

void CameraEffectsController::OnCameraEffectChanged(
    const cros::mojom::EffectsConfigPtr& new_effects) {
  // As `CameraHalDispatcher` notifies the `new_effects` from a different
  // thread, we want to ensure the `current_effects_` is always accessed through
  // the `main_task_runner_`.
  if (!main_task_runner_->RunsTasksInCurrentSequence()) {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CameraEffectsController::OnCameraEffectChanged,
                       weak_factory_.GetWeakPtr(), new_effects.Clone()));
    return;
  }

  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  // If `SetCamerEffects()` finished, update `current_effects_` and prefs.
  if (!new_effects.is_null()) {
    SetEffectsConfigToPref(new_effects.Clone());
    current_effects_ = new_effects.Clone();
  }
}

void CameraEffectsController::OnVideoConferenceBubbleOpened() {
  const bool is_eligible = IsEligibleForBackgroundReplace();
  const bool is_enterprise_disabled = !IsVcBackgroundAllowedByEnterprise();

  // If the updated eligible state is false, no further action required.
  if (!is_eligible) {
    return;
  }

  // If the background replace is already eligibled but no changes in enterprise
  // enabled state, no further action required.
  if (is_eligible_for_background_replace_ &&
      is_enterprise_disabled == is_background_replace_disabled_by_enterprise_) {
    return;
  }

  // If background blur effect not yet added, do nothing.
  if (!GetEffectById(VcEffectId::kBackgroundBlur)) {
    return;
  }

  // Update Background Blur effect if background replace eligible state changes
  // from false -> true or enterprise enabled state changes.
  is_eligible_for_background_replace_ = true;
  is_background_replace_disabled_by_enterprise_ = is_enterprise_disabled;
  RemoveEffect(VcEffectId::kBackgroundBlur);
  AddBackgroundBlurEffect();
}

void CameraEffectsController::OnAutozoomControlEnabledChanged(bool enabled) {
  if (!enabled) {
    RemoveEffect(VcEffectId::kCameraFraming);
    return;
  }

  // Using `base::Unretained()` is safe here since `this` owns the created
  // VcHostedEffect after calling `AddEffect()` below.
  std::unique_ptr<VcHostedEffect> effect = std::make_unique<VcHostedEffect>(
      /*type=*/VcEffectType::kToggle,
      /*get_state_callback=*/
      base::BindRepeating(&CameraEffectsController::GetEffectState,
                          base::Unretained(this), VcEffectId::kCameraFraming),
      /*effect_id=*/VcEffectId::kCameraFraming);

  auto effect_state = std::make_unique<VcEffectState>(
      /*icon=*/&kVideoConferenceCameraFramingOnIcon,
      /*label_text=*/
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_AUTOZOOM_BUTTON_LABEL),
      /*accessible_name_id=*/
      IDS_ASH_STATUS_TRAY_AUTOZOOM_BUTTON_LABEL,
      /*button_callback=*/
      base::BindRepeating(&CameraEffectsController::OnEffectControlActivated,
                          base::Unretained(this),
                          /*effect_id=*/VcEffectId::kCameraFraming,
                          /*value=*/std::nullopt));
  effect->AddState(std::move(effect_state));

  effect->set_dependency_flags(VcHostedEffect::ResourceDependency::kCamera);
  AddEffect(std::move(effect));
  auto& effects_manager =
      VideoConferenceTrayController::Get()->GetEffectsManager();
  if (!effects_manager.IsDelegateRegistered(this)) {
    effects_manager.RegisterDelegate(this);
  }
}

cros::mojom::SegmentationModel
CameraEffectsController::GetSegmentationModelType() {
  cros::mojom::SegmentationModel model_type =
      cros::mojom::SegmentationModel::kHighResolution;
  const std::string segmentation_model_param = GetFieldTrialParamValueByFeature(
      ash::features::kVcSegmentationModel, "segmentation_model");

  if (segmentation_model_param == "lower_resolution") {
    model_type = cros::mojom::SegmentationModel::kLowerResolution;
  }

  return model_type;
}

void CameraEffectsController::SetCameraEffects(
    cros::mojom::EffectsConfigPtr config,
    bool is_initialization,
    base::OnceCallback<void(bool)> copy_background_image_complete_callback) {
  // For backwards compatibility, will be removed after mojom is updated.
  if (config->blur_enabled) {
    config->effect = cros::mojom::CameraEffect::kBackgroundBlur;
  }
  if (config->replace_enabled) {
    config->effect = cros::mojom::CameraEffect::kBackgroundReplace;
  }
  if (config->relight_enabled) {
    config->effect = cros::mojom::CameraEffect::kPortraitRelight;
  }

  // Update effects config with settings from feature flags.
  config->segmentation_model = GetSegmentationModelType();
  double intensity = GetFieldTrialParamByFeatureAsDouble(
      ash::features::kVcLightIntensity, "light_intensity", -1.0);
  // Only set if its overridden by flags, otherwise use default from lib.
  if (intensity > 0.0) {
    config->light_intensity = intensity;
  }

  config->segmentation_inference_backend =
      GetInferenceBackend(ash::features::kVcSegmentationInferenceBackend);
  config->relighting_inference_backend =
      GetInferenceBackend(ash::features::kVcRelightingInferenceBackend);

  if (config->replace_enabled &&
      config->background_filepath != current_effects_->background_filepath) {
    const base::FilePath background_image_filepath =
        camera_background_img_dir_.Append(config->background_filepath.value());
    const base::FilePath background_run_filepath =
        camera_background_run_dir_.Append(config->background_filepath.value());

    // Copy image file on the worker thread first.
    blocking_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&CopyBackgroundImageFile, background_image_filepath,
                       background_run_filepath),
        base::BindOnce(
            &CameraEffectsController::OnCopyBackgroundImageFileComplete,
            weak_factory_.GetWeakPtr(), std::move(config), is_initialization,
            std::move(copy_background_image_complete_callback)));
    return;
  }

  SetCameraEffectsInCameraHalDispatcherImpl(std::move(config));
}

void CameraEffectsController::OnCopyBackgroundImageFileComplete(
    cros::mojom::EffectsConfigPtr new_config,
    bool is_initialization,
    base::OnceCallback<void(bool)> copy_background_image_complete_callback,
    bool copy_succeeded) {
  std::move(copy_background_image_complete_callback).Run(copy_succeeded);

  // If copy_succeeded, continue to apply all effects.
  if (copy_succeeded) {
    new_config->blur_enabled = false;
    SetCameraEffectsInCameraHalDispatcherImpl(std::move(new_config));
    return;
  }

  // If copy_succeeded is false, but is_initialization is true, then apply the
  // rest of the effefcts. We only want to continue when it is initialization,
  // because we don't want to randomly turn off the user's background effects
  // due to the failure of copying the new image file.
  if (is_initialization) {
    new_config->replace_enabled = false;
    new_config->background_filepath.reset();
    SetCameraEffectsInCameraHalDispatcherImpl(std::move(new_config));
  }
}

void CameraEffectsController::OnSaveBackgroundImageFileComplete(
    base::OnceCallback<void(bool)> callback,
    const base::FilePath& basename) {
  if (basename.empty()) {
    LOG(ERROR) << "Failed to write the image file: " << basename;
    std::move(callback).Run(false);
    return;
  }

  SetBackgroundImage(basename, std::move(callback));
}

cros::mojom::EffectsConfigPtr
CameraEffectsController::GetEffectsConfigFromPref() {
  cros::mojom::EffectsConfigPtr effects = cros::mojom::EffectsConfig::New();
  if (!pref_change_registrar_ || !pref_change_registrar_->prefs()) {
    return effects;
  }

  int background_blur_state_in_pref =
      pref_change_registrar_->prefs()->GetInteger(prefs::kBackgroundBlur);
  if (!IsValidBackgroundBlurPrefValue(background_blur_state_in_pref)) {
    LOG(ERROR) << __FUNCTION__ << " background_blur_state_in_pref "
               << background_blur_state_in_pref
               << " is NOT a valid background blur effect state, using kOff";
    background_blur_state_in_pref = BackgroundBlurPrefValue::kOff;
  }

  CameraHalBackgroundBlurState blur_state =
      MapBackgroundBlurPrefValueToCameraHalState(background_blur_state_in_pref);
  effects->blur_enabled = blur_state.second;
  effects->blur_level = blur_state.first;

  if (is_eligible_for_background_replace_) {
    effects->replace_enabled =
        pref_change_registrar_->prefs()->GetBoolean(prefs::kBackgroundReplace);
    if (effects->replace_enabled) {
      effects->background_filepath =
          pref_change_registrar_->prefs()->GetFilePath(
              prefs::kBackgroundImagePath);
    }
  }

  effects->relight_enabled =
      pref_change_registrar_->prefs()->GetBoolean(prefs::kPortraitRelighting);
  effects->retouch_enabled =
      pref_change_registrar_->prefs()->GetBoolean(prefs::kFaceRetouch);
  effects->studio_look_enabled =
      pref_change_registrar_->prefs()->GetBoolean(prefs::kStudioLook);
  return effects;
}

void CameraEffectsController::SetEffectsConfigToPref(
    cros::mojom::EffectsConfigPtr new_config) {
  if (!pref_change_registrar_ || !pref_change_registrar_->prefs()) {
    return;
  }

  if (new_config->blur_enabled != current_effects_->blur_enabled ||
      new_config->blur_level != current_effects_->blur_level) {
    pref_change_registrar_->prefs()->SetInteger(
        prefs::kBackgroundBlur,
        MapBackgroundBlurCameraHalStateToPrefValue(new_config->blur_level,
                                                   new_config->blur_enabled));
  }

  if (is_eligible_for_background_replace_) {
    if (new_config->replace_enabled != current_effects_->replace_enabled) {
      pref_change_registrar_->prefs()->SetBoolean(prefs::kBackgroundReplace,
                                                  new_config->replace_enabled);
    }

    if (new_config->background_filepath !=
        current_effects_->background_filepath) {
      pref_change_registrar_->prefs()->SetFilePath(
          prefs::kBackgroundImagePath,
          new_config->background_filepath.value_or(base::FilePath()));
    }
  }

  if (new_config->relight_enabled != current_effects_->relight_enabled) {
    pref_change_registrar_->prefs()->SetBoolean(prefs::kPortraitRelighting,
                                                new_config->relight_enabled);
  }

  if (new_config->retouch_enabled != current_effects_->retouch_enabled) {
    pref_change_registrar_->prefs()->SetBoolean(prefs::kFaceRetouch,
                                                new_config->retouch_enabled);
  }

  if (new_config->studio_look_enabled !=
      current_effects_->studio_look_enabled) {
    pref_change_registrar_->prefs()->SetBoolean(
        prefs::kStudioLook, new_config->studio_look_enabled);
  }
}

bool CameraEffectsController::IsEffectControlAvailable(
    cros::mojom::CameraEffect effect /* = cros::mojom::CameraEffect::kNone*/) {
  switch (effect) {
    case cros::mojom::CameraEffect::kNone:
    case cros::mojom::CameraEffect::kBackgroundBlur:
      return features::IsVideoConferenceEnabled();
    case cros::mojom::CameraEffect::kPortraitRelight:
      return features::IsVcPortraitRelightEnabled();
    case cros::mojom::CameraEffect::kBackgroundReplace:
      return features::IsVcBackgroundReplaceEnabled();
  }
}

void CameraEffectsController::InitializeEffectControls() {
  if (VideoConferenceTrayController::Get()
          ->GetEffectsManager()
          .IsDelegateRegistered(this)) {
    return;
  }

  AddBackgroundBlurEffect();

  // If portrait relight UI controls are present, construct the effect and its
  // state. If the Studio Look feature is available, the same UI control is used
  // for Studio Look.
  if (IsEffectControlAvailable(cros::mojom::CameraEffect::kPortraitRelight)) {
    auto effect_id = features::IsVcStudioLookEnabled()
                         ? VcEffectId::kStudioLook
                         : VcEffectId::kPortraitRelighting;
    std::unique_ptr<VcHostedEffect> effect = std::make_unique<VcHostedEffect>(
        /*type=*/VcEffectType::kToggle,
        /*get_state_callback=*/
        base::BindRepeating(&CameraEffectsController::GetEffectState,
                            base::Unretained(this), effect_id),
        effect_id);

    const base::CommandLine* command_line =
        base::CommandLine::ForCurrentProcess();
    std::string face_retouch_override = command_line->GetSwitchValueASCII(
        media::switches::kFaceRetouchOverride);
    bool show_studio_look_ui =
        face_retouch_override ==
            media::switches::kFaceRetouchForceEnabledWithRelighting ||
        face_retouch_override ==
            media::switches::kFaceRetouchForceEnabledWithoutRelighting ||
        features::IsVcStudioLookEnabled();

    auto effect_state = std::make_unique<VcEffectState>(
        /*icon=*/show_studio_look_ui ? &kVideoConferenceStudioLookIcon
                                     : &kVideoConferencePortraitRelightOnIcon,
        /*label_text=*/
        l10n_util::GetStringUTF16(
            show_studio_look_ui
                ? IDS_ASH_VIDEO_CONFERENCE_BUBBLE_STUDIO_LOOK_NAME
                : IDS_ASH_VIDEO_CONFERENCE_BUBBLE_PORTRAIT_RELIGHT_NAME),
        /*accessible_name_id=*/
        show_studio_look_ui
            ? IDS_ASH_VIDEO_CONFERENCE_BUBBLE_STUDIO_LOOK_NAME
            : IDS_ASH_VIDEO_CONFERENCE_BUBBLE_PORTRAIT_RELIGHT_NAME,
        /*button_callback=*/
        base::BindRepeating(&CameraEffectsController::OnEffectControlActivated,
                            base::Unretained(this), effect_id,
                            /*value=*/std::nullopt));
    effect->AddState(std::move(effect_state));

    effect->set_dependency_flags(VcHostedEffect::ResourceDependency::kCamera);
    AddEffect(std::move(effect));
  }

  // If *any* effects' UI controls are present, register with the effects
  // manager.
  if (IsEffectControlAvailable()) {
    VideoConferenceTrayController::Get()->GetEffectsManager().RegisterDelegate(
        this);
  }
}

void CameraEffectsController::AddBackgroundBlurEffect() {
  if (!IsEffectControlAvailable(cros::mojom::CameraEffect::kBackgroundBlur)) {
    return;
  }
  // If background blur UI controls are present, construct the effect and its
  // states.
  auto effect = std::make_unique<VcHostedEffect>(
      /*type=*/VcEffectType::kSetValue,
      /*get_state_callback=*/
      base::BindRepeating(&CameraEffectsController::GetEffectState,
                          base::Unretained(this), VcEffectId::kBackgroundBlur),
      /*effect_id=*/VcEffectId::kBackgroundBlur);
  effect->set_label_text(l10n_util::GetStringUTF16(
      IDS_ASH_VIDEO_CONFERENCE_BUBBLE_BACKGROUND_BLUR_NAME));
  effect->set_effects_delegate(this);
  AddBackgroundBlurStateToEffect(
      effect.get(), kVideoConferenceBackgroundBlurOffIcon,
      /*state_value=*/BackgroundBlurPrefValue::kOff,
      /*string_id=*/IDS_ASH_VIDEO_CONFERENCE_BUBBLE_BACKGROUND_BLUR_OFF,
      video_conference::BubbleViewID::kBackgroundBlurOffButton,
      /*is_disabled_by_enterprise=*/false);
  AddBackgroundBlurStateToEffect(
      effect.get(), kVideoConferenceBackgroundBlurLightIcon,
      /*state_value=*/BackgroundBlurPrefValue::kLight,
      /*string_id=*/IDS_ASH_VIDEO_CONFERENCE_BUBBLE_BACKGROUND_BLUR_LIGHT,
      video_conference::BubbleViewID::kBackgroundBlurLightButton,
      /*is_disabled_by_enterprise=*/false);
  AddBackgroundBlurStateToEffect(
      effect.get(), kVideoConferenceBackgroundBlurMaximumIcon,
      /*state_value=*/BackgroundBlurPrefValue::kMaximum,
      /*string_id=*/
      IDS_ASH_VIDEO_CONFERENCE_BUBBLE_BACKGROUND_BLUR_FULL,
      video_conference::BubbleViewID::kBackgroundBlurFullButton,
      /*is_disabled_by_enterprise=*/false);

  if (is_eligible_for_background_replace_) {
    AddBackgroundBlurStateToEffect(
        effect.get(), kAiImageIcon,
        /*state_value=*/BackgroundBlurPrefValue::kImage,
        /*string_id=*/
        IDS_ASH_VIDEO_CONFERENCE_BUBBLE_BACKGROUND_BLUR_IMAGE,
        video_conference::BubbleViewID::kBackgroundBlurImageButton,
        /*is_disabled_by_enterprise=*/
        is_background_replace_disabled_by_enterprise_);
  }
  effect->set_dependency_flags(VcHostedEffect::ResourceDependency::kCamera);
  AddEffect(std::move(effect));
}

void CameraEffectsController::AddBackgroundBlurStateToEffect(
    VcHostedEffect* effect,
    const gfx::VectorIcon& icon,
    int state_value,
    int string_id,
    int view_id,
    bool is_disabled_by_enterprise) {
  DCHECK(effect);
  effect->AddState(std::make_unique<VcEffectState>(
      &icon,
      /*label_text=*/l10n_util::GetStringUTF16(string_id),
      /*accessible_name_id=*/string_id,
      /*button_callback=*/
      base::BindRepeating(&CameraEffectsController::OnEffectControlActivated,
                          weak_factory_.GetWeakPtr(),
                          /*effect_id=*/VcEffectId::kBackgroundBlur,
                          /*value=*/state_value),
      /*state=*/state_value, view_id, is_disabled_by_enterprise));
}

void CameraEffectsController::SetCameraEffectsInCameraHalDispatcherImpl(
    cros::mojom::EffectsConfigPtr config) {
  // Directly calls the callback for testing case.
  if (in_testing_mode_) {
    CHECK_IS_TEST();
    OnCameraEffectChanged(std::move(config));
  } else {
    media::CameraHalDispatcherImpl::GetInstance()->SetCameraEffects(
        std::move(config));
  }
}

}  // namespace ash
