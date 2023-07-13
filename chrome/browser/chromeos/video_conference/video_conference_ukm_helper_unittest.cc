// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/video_conference/video_conference_ukm_helper.h"

#include <memory>
#include <vector>

#include "base/logging.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/video_conference/video_conference_manager_client_common.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_task_environment.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace video_conference {

namespace {
constexpr int kDuration10ms = 10;
constexpr int kDuration17ms = 17;
constexpr int kDuration20ms = 20;
constexpr int kDuration24ms = 24;
constexpr int kDuration70ms = 70;
}  // namespace

using UkmEntry = ukm::builders::VideoConferencingEvent;

class FakeVideoConferenceUkmHelper : public VideoConferenceUkmHelper {
 public:
  explicit FakeVideoConferenceUkmHelper(ukm::TestUkmRecorder* ukm_recorder)
      : VideoConferenceUkmHelper(ukm_recorder,
                                 /*source_id=*/ukm_recorder->GetNewSourceID()) {
  }

  FakeVideoConferenceUkmHelper(const FakeVideoConferenceUkmHelper&) = delete;
  FakeVideoConferenceUkmHelper& operator=(const FakeVideoConferenceUkmHelper&) =
      delete;

  ukm::SourceId source_id() const { return source_id_; }
};

class VideoConferenceUkmHelperTest : public testing::Test {
 public:
  VideoConferenceUkmHelperTest() = default;

  VideoConferenceUkmHelperTest(const VideoConferenceUkmHelperTest&) = delete;
  VideoConferenceUkmHelperTest& operator=(const VideoConferenceUkmHelperTest&) =
      delete;
  ~VideoConferenceUkmHelperTest() override = default;

  void UpdateCapturing(FakeVideoConferenceUkmHelper* ukm_helper,
                       VideoConferenceMediaType device,
                       bool is_capturing) {
    // The update to the helper has to be sent before updating the state.
    ukm_helper->RegisterCapturingUpdate(device, is_capturing);
  }

  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// Tests capturing a media device correctly sets DidCapture<Device> metrics
// values in recorded entries.
TEST_F(VideoConferenceUkmHelperTest, DidCaptureDevice) {
  ukm::TestUkmRecorder ukm_recorder;

  // Create a FakeVideoConferenceUkmHelper
  std::unique_ptr<FakeVideoConferenceUkmHelper> ukm_helper1 =
      std::make_unique<FakeVideoConferenceUkmHelper>(&ukm_recorder);

  // Start capturing device(s).
  UpdateCapturing(ukm_helper1.get(), VideoConferenceMediaType::kCamera, true);

  // Wait 10 milliseconds.
  task_environment().FastForwardBy(base::Milliseconds(kDuration10ms));

  // Stop capturing all devices.
  UpdateCapturing(ukm_helper1.get(), VideoConferenceMediaType::kCamera, false);

  // Destroy ukm_helper1 thus triggering it to record metrics in its destructor.
  ukm_helper1.reset();

  ukm_recorder.ExpectEntryMetric(
      ukm_recorder.GetEntriesByName(UkmEntry::kEntryName)[0],
      UkmEntry::kDidCaptureCameraName, true);
  ukm_recorder.ExpectEntryMetric(
      ukm_recorder.GetEntriesByName(UkmEntry::kEntryName)[0],
      UkmEntry::kDidCaptureMicrophoneName, false);
  ukm_recorder.ExpectEntryMetric(
      ukm_recorder.GetEntriesByName(UkmEntry::kEntryName)[0],
      UkmEntry::kDidCaptureScreenName, false);

  // Create a FakeVideoConferenceUkmHelper
  std::unique_ptr<FakeVideoConferenceUkmHelper> ukm_helper2 =
      std::make_unique<FakeVideoConferenceUkmHelper>(&ukm_recorder);

  // Start capturing all devices.
  UpdateCapturing(ukm_helper2.get(), VideoConferenceMediaType::kCamera, true);
  UpdateCapturing(ukm_helper2.get(), VideoConferenceMediaType::kMicrophone,
                  true);

  // Wait 10 milliseconds.
  task_environment().FastForwardBy(base::Milliseconds(kDuration10ms));

  // Stop capturing all devices.
  UpdateCapturing(ukm_helper2.get(), VideoConferenceMediaType::kCamera, false);
  UpdateCapturing(ukm_helper2.get(), VideoConferenceMediaType::kMicrophone,
                  false);

  // Destroy ukm_helper2 thus triggering it to record metrics in its destructor.
  ukm_helper2.reset();

  ukm_recorder.ExpectEntryMetric(
      ukm_recorder.GetEntriesByName(UkmEntry::kEntryName)[1],
      UkmEntry::kDidCaptureCameraName, true);
  ukm_recorder.ExpectEntryMetric(
      ukm_recorder.GetEntriesByName(UkmEntry::kEntryName)[1],
      UkmEntry::kDidCaptureMicrophoneName, true);
  ukm_recorder.ExpectEntryMetric(
      ukm_recorder.GetEntriesByName(UkmEntry::kEntryName)[1],
      UkmEntry::kDidCaptureScreenName, false);

  // Create a FakeVideoConferenceUkmHelper
  std::unique_ptr<FakeVideoConferenceUkmHelper> ukm_helper3 =
      std::make_unique<FakeVideoConferenceUkmHelper>(&ukm_recorder);

  // Start capturing all devices.
  UpdateCapturing(ukm_helper3.get(), VideoConferenceMediaType::kCamera, true);
  UpdateCapturing(ukm_helper3.get(), VideoConferenceMediaType::kMicrophone,
                  true);
  UpdateCapturing(ukm_helper3.get(), VideoConferenceMediaType::kScreen, true);

  // Wait 10 milliseconds.
  task_environment().FastForwardBy(base::Milliseconds(kDuration10ms));

  // Stop capturing all devices.
  UpdateCapturing(ukm_helper3.get(), VideoConferenceMediaType::kCamera, false);
  UpdateCapturing(ukm_helper3.get(), VideoConferenceMediaType::kMicrophone,
                  false);
  UpdateCapturing(ukm_helper3.get(), VideoConferenceMediaType::kScreen, false);

  // Destroy ukm_helper3 thus triggering it to record metrics in its destructor.
  ukm_helper3.reset();

  ukm_recorder.ExpectEntryMetric(
      ukm_recorder.GetEntriesByName(UkmEntry::kEntryName)[2],
      UkmEntry::kDidCaptureCameraName, true);
  ukm_recorder.ExpectEntryMetric(
      ukm_recorder.GetEntriesByName(UkmEntry::kEntryName)[2],
      UkmEntry::kDidCaptureMicrophoneName, true);
  ukm_recorder.ExpectEntryMetric(
      ukm_recorder.GetEntriesByName(UkmEntry::kEntryName)[2],
      UkmEntry::kDidCaptureScreenName, true);
}

// Tests media device capture durations.
TEST_F(VideoConferenceUkmHelperTest, CaptureDurations) {
  ukm::TestUkmRecorder ukm_recorder;

  // Create a FakeVideoConferenceUkmHelper
  std::unique_ptr<FakeVideoConferenceUkmHelper> ukm_helper =
      std::make_unique<FakeVideoConferenceUkmHelper>(&ukm_recorder);

  // Start and stop capturing camera.
  UpdateCapturing(ukm_helper.get(), VideoConferenceMediaType::kCamera, true);
  task_environment().FastForwardBy(base::Milliseconds(kDuration17ms));
  UpdateCapturing(ukm_helper.get(), VideoConferenceMediaType::kCamera, false);

  // Start and stop capturing microphone.
  UpdateCapturing(ukm_helper.get(), VideoConferenceMediaType::kMicrophone,
                  true);
  task_environment().FastForwardBy(base::Milliseconds(kDuration24ms));
  UpdateCapturing(ukm_helper.get(), VideoConferenceMediaType::kMicrophone,
                  false);

  // Start and stop capturing screen.
  UpdateCapturing(ukm_helper.get(), VideoConferenceMediaType::kScreen, true);
  task_environment().FastForwardBy(base::Milliseconds(kDuration70ms));
  UpdateCapturing(ukm_helper.get(), VideoConferenceMediaType::kScreen, false);

  // Destroy ukm_helper thus triggering it to record metrics in its destructor.
  ukm_helper.reset();

  // Here the expected value is 10ms even though the true duration was 17ms due
  // to all durations being bucketed using
  // `ukm::GetSemanticBucketMinForDurationTiming`.
  ukm_recorder.ExpectEntryMetric(
      ukm_recorder.GetEntriesByName(UkmEntry::kEntryName)[0],
      UkmEntry::kCameraCaptureDurationName, kDuration10ms);
  // Here again the expected duration is 20ms instead of 24ms to test bucketing.
  ukm_recorder.ExpectEntryMetric(
      ukm_recorder.GetEntriesByName(UkmEntry::kEntryName)[0],
      UkmEntry::kMicrophoneCaptureDurationName, kDuration20ms);
  ukm_recorder.ExpectEntryMetric(
      ukm_recorder.GetEntriesByName(UkmEntry::kEntryName)[0],
      UkmEntry::kScreenCaptureDurationName, kDuration70ms);
}

// Tests total duration equals the time duration from the creation of the
// VideoConferenceUkmHelper to its destruction.
TEST_F(VideoConferenceUkmHelperTest, TotalDuration) {
  ukm::TestUkmRecorder ukm_recorder;

  // Create a FakeVideoConferenceUkmHelper
  std::unique_ptr<FakeVideoConferenceUkmHelper> ukm_helper =
      std::make_unique<FakeVideoConferenceUkmHelper>(&ukm_recorder);

  // Start and stop capturing camera.
  UpdateCapturing(ukm_helper.get(), VideoConferenceMediaType::kCamera, true);
  task_environment().FastForwardBy(base::Milliseconds(kDuration10ms));
  UpdateCapturing(ukm_helper.get(), VideoConferenceMediaType::kCamera, false);

  // Start and stop capturing microphone.
  UpdateCapturing(ukm_helper.get(), VideoConferenceMediaType::kMicrophone,
                  true);
  task_environment().FastForwardBy(base::Milliseconds(kDuration20ms));
  UpdateCapturing(ukm_helper.get(), VideoConferenceMediaType::kMicrophone,
                  false);

  // Start and stop capturing screen.
  UpdateCapturing(ukm_helper.get(), VideoConferenceMediaType::kScreen, true);
  task_environment().FastForwardBy(base::Milliseconds(kDuration70ms));
  UpdateCapturing(ukm_helper.get(), VideoConferenceMediaType::kScreen, false);

  // Destroy ukm_helper thus triggering it to record metrics in its destructor.
  ukm_helper.reset();

  ukm_recorder.ExpectEntryMetric(
      ukm_recorder.GetEntriesByName(UkmEntry::kEntryName)[0],
      UkmEntry::kTotalDurationName,
      kDuration10ms + kDuration20ms + kDuration70ms);
}

// Tests that the destructor correctly includes any pending media devices whose
// durations were being tracked even if a stop capture signal did not arrive for
// them.
TEST_F(VideoConferenceUkmHelperTest, DestructorIncludesPendingCaptures) {
  ukm::TestUkmRecorder ukm_recorder;

  // Create a FakeVideoConferenceUkmHelper
  std::unique_ptr<FakeVideoConferenceUkmHelper> ukm_helper =
      std::make_unique<FakeVideoConferenceUkmHelper>(&ukm_recorder);

  // Start capturing media devices
  UpdateCapturing(ukm_helper.get(), VideoConferenceMediaType::kCamera, true);
  UpdateCapturing(ukm_helper.get(), VideoConferenceMediaType::kMicrophone,
                  true);
  UpdateCapturing(ukm_helper.get(), VideoConferenceMediaType::kScreen, true);

  // Wait
  task_environment().FastForwardBy(base::Milliseconds(kDuration20ms));

  // Destroy the VideoConferenceUkmHelper and trigger its destructor.
  ukm_helper.reset();

  ukm_recorder.ExpectEntryMetric(
      ukm_recorder.GetEntriesByName(UkmEntry::kEntryName)[0],
      UkmEntry::kCameraCaptureDurationName, kDuration20ms);
  ukm_recorder.ExpectEntryMetric(
      ukm_recorder.GetEntriesByName(UkmEntry::kEntryName)[0],
      UkmEntry::kMicrophoneCaptureDurationName, kDuration20ms);
  ukm_recorder.ExpectEntryMetric(
      ukm_recorder.GetEntriesByName(UkmEntry::kEntryName)[0],
      UkmEntry::kScreenCaptureDurationName, kDuration20ms);
  ukm_recorder.ExpectEntryMetric(
      ukm_recorder.GetEntriesByName(UkmEntry::kEntryName)[0],
      UkmEntry::kTotalDurationName, kDuration20ms);
}

}  // namespace video_conference
