// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/app/app_browser_test_base.h"

#include <utility>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ttc/app/audio_controller.h"
#include "chrome/browser/ttc/app/conversation_impl.h"
#include "chrome/browser/ttc/app/public/conversation.h"
#include "chrome/browser/ttc/app/public/make_conversation.h"
#include "chrome/browser/ttc/app/test_utils.h"
#include "chrome/browser/ttc/core/features.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"
#include "chrome/browser/ttc/core/ttc_keyed_service_factory.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "media/audio/audio_system_impl.h"
#include "media/audio/mock_audio_manager.h"
#include "media/audio/test_audio_thread.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ttc {

AppBrowserTestBase::AppBrowserTestBase() {
  scoped_feature_list_.InitAndEnableFeature(kTtc);
}

AppBrowserTestBase::~AppBrowserTestBase() = default;

void AppBrowserTestBase::SetUpBrowserContextKeyedServices(
    content::BrowserContext* context) {
  PlatformBrowserTest::SetUpBrowserContextKeyedServices(context);

  // Replace the service with one whose sessions use a MockTtcBackend.
  TtcKeyedServiceFactory::GetInstance()->SetTestingFactory(
      context,
      base::BindLambdaForTesting([this](content::BrowserContext* context)
                                     -> std::unique_ptr<KeyedService> {
        return std::make_unique<TtcKeyedService>(
            Profile::FromBrowserContext(context),
            base::BindRepeating(&AppBrowserTestBase::MakeConversation,
                                base::Unretained(this)));
      }));
}

void AppBrowserTestBase::SetUpOnMainThread() {
  PlatformBrowserTest::SetUpOnMainThread();
  embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(embedded_test_server()->Start());

  audio_manager_ = std::make_unique<media::MockAudioManager>(
      std::make_unique<media::TestAudioThread>());
  audio_manager_->SetHasInputDevices(true);
  audio_manager_->SetInputStreamParameters(
      media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                             media::ChannelLayoutConfig::Mono(),
                             /*sample_rate=*/48000,
                             /*frames_per_buffer=*/480));
}

void AppBrowserTestBase::TearDownOnMainThread() {
  // The session owns the AudioController so it must go away before the audio
  // manager it's using.
  ttc_service().EndSession();
  conversation_ = nullptr;

  audio_manager_->Shutdown();
  audio_manager_.reset();

  PlatformBrowserTest::TearDownOnMainThread();
}

Profile* AppBrowserTestBase::profile() {
  return chrome_test_utils::GetProfile(this);
}

content::WebContents* AppBrowserTestBase::web_contents() {
  return chrome_test_utils::GetActiveWebContents(this);
}

TtcKeyedService& AppBrowserTestBase::ttc_service() {
  return CHECK_DEREF(TtcKeyedService::Get(profile()));
}

ConversationImpl* AppBrowserTestBase::conversation() {
  return ttc_service().is_session_active() ? conversation_.get() : nullptr;
}

MockTtcBackend* AppBrowserTestBase::backend() {
  ConversationImpl* conversation_impl = conversation();
  return conversation_impl
             ? static_cast<MockTtcBackend*>(conversation_impl->backend())
             : nullptr;
}

std::unique_ptr<Conversation> AppBrowserTestBase::MakeConversation(
    SessionController& session_controller) {
  // Dropping the receiver in the binder means no real capture stream is ever
  // opened.
  auto audio_controller = std::make_unique<AudioController>(
      base::BindRepeating(
          [](mojo::PendingReceiver<media::mojom::AudioStreamFactory>) {}),
      base::BindLambdaForTesting(
          [this]() -> std::unique_ptr<media::AudioSystem> {
            return std::make_unique<media::AudioSystemImpl>(
                audio_manager_.get());
          }));

  std::unique_ptr<Conversation> conversation = MakeConversationImplForTesting(
      session_controller, std::make_unique<testing::NiceMock<MockTtcBackend>>(),
      std::move(audio_controller));
  conversation_ = static_cast<ConversationImpl*>(conversation.get());
  return conversation;
}

}  // namespace ttc
