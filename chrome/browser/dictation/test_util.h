// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_TEST_UTIL_H_
#define CHROME_BROWSER_DICTATION_TEST_UTIL_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/dictation/dictation_context.h"
#include "chrome/browser/dictation/dictation_keyed_service.h"
#include "chrome/browser/dictation/dictation_multiplexer.h"
#include "chrome/browser/dictation/session_controller_delegate.h"
#include "chrome/browser/dictation/session_ui.h"
#include "chrome/browser/dictation/stream_provider.h"
#include "chrome/browser/dictation/target.h"
#include "chrome/common/extensions/api/dictation_private.h"
#include "testing/gmock/include/gmock/gmock.h"

class Profile;

namespace content {
class WebContents;
}

namespace extensions {
class Extension;
}

namespace dictation {

inline constexpr std::string_view kDictationTestExtensionId =
    "dfihfgggpgemecjdjahibncmmjlfjggp";

// A target ID that doesn't point to anything. Used only in unit tests where the
// target isn't actually used.
TargetId EmptyTargetId();

// Returns a target that points to the default element in the page. Currently
// this will be the focused element in the primary main frame.
TargetId DefaultInPageTargetId(content::WebContents* web_contents);

// Returns a ScopedFeatureList that enables Dictation with common params for
// testing.
base::test::ScopedFeatureList CreateEnablingFeatureList();

// Loads an extension that provides an implementation of the connector
// extension in a "manual" mode usable from tests which prevents the extension
// from starting the speech API or responding to any events. Tests using this
// will manually simulate API calls from the extension using the send methods
// below.
const extensions::Extension* LoadTestExtensionInManualMode(Profile* profile);

// Simulates the connector extension sending a transcript update and blocks
// until the browser process processes the API call.
void ExtensionSendTranscriptUpdate(
    Profile* profile,
    DictationMultiplexer::StreamId stream_id,
    extensions::api::dictation_private::TranscriptionType type,
    std::string_view data);

// Simulates the connector extension sending a stream state update and blocks
// until the browser process processes the API call.
void ExtensionSendStreamStateUpdate(
    Profile* profile,
    DictationMultiplexer::StreamId stream_id,
    extensions::api::dictation_private::StreamState state);

// Blocks until the extension has received the OnStartStream event for the given
// stream ID.
void ExtensionWaitForStreamStart(Profile* profile,
                                 DictationMultiplexer::StreamId stream_id);

// Blocks until the extension has received the OnStartStream event for the given
// stream ID, and returns the DictationContext containing the page context
// passed to the extension, or nullopt if no context was passed.
std::optional<DictationContext> ExtensionGetStartStreamDetails(
    Profile* profile,
    DictationMultiplexer::StreamId stream_id);

// Blocks until the extension has received the OnContextUpdate event for the
// given stream ID, and returns the DictationContext containing the page context
// passed to the extension.
DictationContext ExtensionGetUpdatedContext(
    Profile* profile,
    DictationMultiplexer::StreamId stream_id);

class MockStreamProvider : public StreamProvider {
 public:
  MockStreamProvider();
  ~MockStreamProvider() override;

  MOCK_METHOD(void,
              BindToTargetAndConnect,
              (std::unique_ptr<Target> target),
              (override));
  MOCK_METHOD(void, Stop, (), (override));
  MOCK_METHOD(void,
              OnTranscriptionUpdated,
              (const std::string& data, bool is_final),
              (override));
  MOCK_METHOD(void, OnStreamStateChanged, (StreamState state), (override));
  MOCK_METHOD(StreamState, GetState, (), (const, override));
  MOCK_METHOD(const Target*, GetTarget, (), (const, override));
};

class MockSessionUi : public SessionUi {
 public:
  MockSessionUi();
  ~MockSessionUi() override;
};

class MockSessionControllerDelegate : public SessionControllerDelegate {
 public:
  MockSessionControllerDelegate();
  ~MockSessionControllerDelegate() override;

  MOCK_METHOD(std::unique_ptr<StreamProvider>,
              CreateStreamProvider,
              (SessionController & controller),
              (const, override));
  MOCK_METHOD(std::unique_ptr<SessionUi>,
              CreateUi,
              (SessionController & controller),
              (const, override));
  MOCK_METHOD(void, EndSession, (), (override));
};

class MockDictationKeyedService : public DictationKeyedService {
 public:
  explicit MockDictationKeyedService(Profile* profile);
  ~MockDictationKeyedService() override;

  // DictationKeyedService:
  std::unique_ptr<StreamProvider> CreateStreamProvider(
      SessionController& controller) const override;
  std::unique_ptr<SessionUi> CreateUi(
      SessionController& controller) const override;
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_TEST_UTIL_H_
