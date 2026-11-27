// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/speaker_volume_warning_detector.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ttc/core/features.h"
#include "chrome/browser/ttc/core/output_volume_monitor.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/ttc/app/public/session_journal.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ttc {
namespace {

using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Key;
using ::testing::Not;
using ::testing::Pair;

constexpr OutputVolumeState kAudible{.muted = false, .level = 0.5f};
constexpr OutputVolumeState kMuted{.muted = true, .level = 0.5f};
constexpr OutputVolumeState kQuiet{.muted = false, .level = 0.05f};

class FakeSessionJournal : public SessionJournal {
 public:
  struct Entry {
    std::string event_name;
    std::vector<std::pair<std::string, std::string>> details;
  };

  void Log(std::string_view event_name,
           std::vector<actor::mojom::JournalDetailsPtr> details,
           Track track) override {
    Entry entry;
    entry.event_name = std::string(event_name);
    for (const auto& detail : details) {
      entry.details.emplace_back(detail->key, detail->value);
    }
    entries_.push_back(std::move(entry));
  }
  void HandleServerJournalEvent(const ServerJournalEvent& event) override {}
  void LogAudio(std::string_view mime_type,
                base::span<const uint8_t> data) override {}
  std::unique_ptr<PendingAsyncEvent> BeginAsyncEvent(
      std::string_view event_name,
      std::vector<actor::mojom::JournalDetailsPtr> details) override {
    return nullptr;
  }

  const std::vector<Entry>& entries() const { return entries_; }

 private:
  std::vector<Entry> entries_;
};

class SpeakerVolumeWarningDetectorTest : public testing::Test {
 protected:
  class FakeOutputVolumeMonitor : public OutputVolumeMonitor {
   public:
    FakeOutputVolumeMonitor(SpeakerVolumeWarningDetectorTest& test,
                            StateChangedCallback callback)
        : test_(test),
          state_(test.initial_state_),
          callback_(std::move(callback)) {
      test_->monitor_ = this;
    }
    ~FakeOutputVolumeMonitor() override { test_->monitor_ = nullptr; }

    std::optional<OutputVolumeState> GetState() const override {
      return state_;
    }

    void SetState(const OutputVolumeState& state) {
      state_ = state;
      callback_.Run(state);
    }

   private:
    const raw_ref<SpeakerVolumeWarningDetectorTest> test_;
    std::optional<OutputVolumeState> state_;
    const StateChangedCallback callback_;
  };

  std::unique_ptr<OutputVolumeMonitor> CreateMonitor(
      OutputVolumeMonitor::StateChangedCallback callback) {
    if (!platform_supported_) {
      return nullptr;
    }
    return std::make_unique<FakeOutputVolumeMonitor>(*this,
                                                     std::move(callback));
  }

  SpeakerVolumeWarningDetector::WarningCallback RecordWarnings() {
    return base::BindRepeating(
        [](std::vector<SpeakerVolumeWarning>* warnings,
           SpeakerVolumeWarning warning) { warnings->push_back(warning); },
        &warnings_);
  }

  void SetVolume(const OutputVolumeState& state) {
    ASSERT_TRUE(monitor_);
    monitor_->SetState(state);
  }

  FakeSessionJournal journal_;
  std::optional<OutputVolumeState> initial_state_ = kAudible;
  bool platform_supported_ = true;
  raw_ptr<FakeOutputVolumeMonitor> monitor_ = nullptr;
  std::vector<SpeakerVolumeWarning> warnings_;
  OutputVolumeMonitor::ScopedFactoryForTesting factory_{
      base::BindRepeating(&SpeakerVolumeWarningDetectorTest::CreateMonitor,
                          base::Unretained(this))};
};

TEST_F(SpeakerVolumeWarningDetectorTest, ReturnsNullWhenPlatformUnsupported) {
  platform_supported_ = false;
  EXPECT_EQ(
      SpeakerVolumeWarningDetector::MaybeCreate(journal_, RecordWarnings()),
      nullptr);
  EXPECT_THAT(journal_.entries(), IsEmpty());
  EXPECT_THAT(warnings_, IsEmpty());
}

TEST_F(SpeakerVolumeWarningDetectorTest, LogsInitialAndChangedVolumeToJournal) {
  std::unique_ptr<SpeakerVolumeWarningDetector> detector =
      SpeakerVolumeWarningDetector::MaybeCreate(journal_, RecordWarnings());
  ASSERT_TRUE(detector);
  SetVolume({.muted = true, .level = std::nullopt});

  ASSERT_EQ(journal_.entries().size(), 2u);
  EXPECT_EQ(journal_.entries()[0].event_name, "TtcSystemSpeakerVolume");
  EXPECT_THAT(journal_.entries()[0].details, Contains(Pair("muted", "false")));
  EXPECT_THAT(journal_.entries()[0].details, Contains(Pair("level", "0.5")));

  EXPECT_EQ(journal_.entries()[1].event_name, "TtcSystemSpeakerVolume");
  EXPECT_THAT(journal_.entries()[1].details, Contains(Pair("muted", "true")));
  EXPECT_THAT(journal_.entries()[1].details, Not(Contains(Key("level"))));
}

TEST_F(SpeakerVolumeWarningDetectorTest, UnknownInitialVolumeIsNotLogged) {
  initial_state_ = std::nullopt;
  std::unique_ptr<SpeakerVolumeWarningDetector> detector =
      SpeakerVolumeWarningDetector::MaybeCreate(journal_, RecordWarnings());
  ASSERT_TRUE(detector);
  EXPECT_THAT(journal_.entries(), IsEmpty());

  SetVolume(kAudible);
  ASSERT_EQ(journal_.entries().size(), 1u);
  EXPECT_EQ(journal_.entries()[0].event_name, "TtcSystemSpeakerVolume");
}

TEST_F(SpeakerVolumeWarningDetectorTest, MutedAtStartWarnsImmediately) {
  initial_state_ = kMuted;
  std::unique_ptr<SpeakerVolumeWarningDetector> detector =
      SpeakerVolumeWarningDetector::MaybeCreate(journal_, RecordWarnings());
  ASSERT_TRUE(detector);
  EXPECT_THAT(warnings_, ElementsAre(SpeakerVolumeWarning::kMuted));
}

TEST_F(SpeakerVolumeWarningDetectorTest, UnknownStateAtStartIsQuiet) {
  initial_state_ = std::nullopt;
  std::unique_ptr<SpeakerVolumeWarningDetector> detector =
      SpeakerVolumeWarningDetector::MaybeCreate(journal_, RecordWarnings());
  ASSERT_TRUE(detector);
  EXPECT_THAT(warnings_, IsEmpty());

  SetVolume(kMuted);
  EXPECT_THAT(warnings_, ElementsAre(SpeakerVolumeWarning::kMuted));
}

// Like the microphone, each transition to muted warns while live.
TEST_F(SpeakerVolumeWarningDetectorTest, EachMuteWarns) {
  std::unique_ptr<SpeakerVolumeWarningDetector> detector =
      SpeakerVolumeWarningDetector::MaybeCreate(journal_, RecordWarnings());
  ASSERT_TRUE(detector);

  SetVolume(kMuted);
  // Still muted: the volume changed, but there is nothing new to warn about.
  SetVolume({.muted = true, .level = 0.8f});
  SetVolume(kAudible);
  SetVolume(kMuted);

  EXPECT_THAT(warnings_, ElementsAre(SpeakerVolumeWarning::kMuted,
                                     SpeakerVolumeWarning::kMuted));
}

// Users may keep the volume low on purpose: warn at most once per session,
// when real model playback audio arrives.
TEST_F(SpeakerVolumeWarningDetectorTest,
       LowVolumeWarnsOnceWhenModelPlaysAudio) {
  initial_state_ = kQuiet;
  std::unique_ptr<SpeakerVolumeWarningDetector> detector =
      SpeakerVolumeWarningDetector::MaybeCreate(journal_, RecordWarnings());
  ASSERT_TRUE(detector);

  // Silent periods and volume changes while no audio plays never warn.
  SetVolume({.muted = false, .level = 0.02f});
  EXPECT_THAT(warnings_, IsEmpty());

  // The first playback audio chunk warns once; later chunks and lower volumes
  // do not warn again.
  detector->OnModelAudioEnvelopeReady();
  detector->OnModelAudioEnvelopeReady();
  SetVolume({.muted = false, .level = 0.01f});
  detector->OnModelAudioEnvelopeReady();
  EXPECT_THAT(warnings_, ElementsAre(SpeakerVolumeWarning::kLow));
}

TEST_F(SpeakerVolumeWarningDetectorTest, VolumeDroppingBeforeModelAudioWarns) {
  initial_state_ = OutputVolumeState{.muted = false, .level = 0.2f};
  std::unique_ptr<SpeakerVolumeWarningDetector> detector =
      SpeakerVolumeWarningDetector::MaybeCreate(journal_, RecordWarnings());
  ASSERT_TRUE(detector);

  // At the threshold (20%) it is not low.
  detector->OnModelAudioEnvelopeReady();
  EXPECT_THAT(warnings_, IsEmpty());

  SetVolume(kQuiet);
  EXPECT_THAT(warnings_, IsEmpty());

  detector->OnModelAudioEnvelopeReady();
  EXPECT_THAT(warnings_, ElementsAre(SpeakerVolumeWarning::kLow));
}

// Muting has its own warning, and a device without volume control is never
// low.
TEST_F(SpeakerVolumeWarningDetectorTest,
       NoLowVolumeWarningWhenMutedOrUncontrolled) {
  initial_state_ = OutputVolumeState{.muted = false, .level = std::nullopt};
  std::unique_ptr<SpeakerVolumeWarningDetector> detector =
      SpeakerVolumeWarningDetector::MaybeCreate(journal_, RecordWarnings());
  ASSERT_TRUE(detector);

  detector->OnModelAudioEnvelopeReady();
  EXPECT_THAT(warnings_, IsEmpty());

  // Muting warns as muted, never as low, even at level 0.
  SetVolume({.muted = true, .level = 0.0f});
  detector->OnModelAudioEnvelopeReady();
  EXPECT_THAT(warnings_, ElementsAre(SpeakerVolumeWarning::kMuted));

  // Muting at 0 did not consume the low-volume warning.
  SetVolume({.muted = false, .level = 0.0f});
  detector->OnModelAudioEnvelopeReady();
  EXPECT_THAT(warnings_, ElementsAre(SpeakerVolumeWarning::kMuted,
                                     SpeakerVolumeWarning::kLow));
}

// A volume at the configured percent is not low, while a volume below it is.
TEST_F(SpeakerVolumeWarningDetectorTest, MinVolumePercentFollowsParam) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kTtcSpeakerVolumeWarning, {{"min_volume_percent", "70"}});
  initial_state_ = OutputVolumeState{.muted = false, .level = 0.7f};
  std::unique_ptr<SpeakerVolumeWarningDetector> detector =
      SpeakerVolumeWarningDetector::MaybeCreate(journal_, RecordWarnings());
  ASSERT_TRUE(detector);

  detector->OnModelAudioEnvelopeReady();
  EXPECT_THAT(warnings_, IsEmpty());

  SetVolume({.muted = false, .level = 0.69f});
  detector->OnModelAudioEnvelopeReady();
  EXPECT_THAT(warnings_, ElementsAre(SpeakerVolumeWarning::kLow));
}

}  // namespace
}  // namespace ttc
