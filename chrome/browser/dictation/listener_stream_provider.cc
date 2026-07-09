// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/listener_stream_provider.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/dictation/dictation_context_fetcher.h"
#include "chrome/browser/dictation/dictation_keyed_service.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/browser/dictation/stream_provider_delegate.h"
#include "chrome/browser/dictation/target.h"
#include "chrome/common/extensions/api/dictation_private.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"

namespace dictation {

namespace {

extensions::api::dictation_private::DictationContext ConvertToApiContext(
    DictationContext result) {
  extensions::api::dictation_private::DictationContext api_context;
  if (result.annotated_page_content.has_value()) {
    std::vector<uint8_t> proto_data(
        result.annotated_page_content->ByteSizeLong());
    if (!proto_data.empty()) {
      result.annotated_page_content->SerializeToArray(proto_data.data(),
                                                      proto_data.size());
    }
    api_context.annotated_page_content = std::move(proto_data);
  }

  api_context.inner_text = std::move(result.inner_text);
  api_context.editable_content = std::move(result.editable_content);
  return api_context;
}

}  // namespace

ListenerStreamProvider::ListenerStreamProvider(
    content::BrowserContext* browser_context,
    StreamProviderDelegate& delegate)
    : delegate_(delegate), browser_context_(browser_context) {}

ListenerStreamProvider::~ListenerStreamProvider() {
  if (stream_id_) {
    GetMultiplexer().UnregisterStreamProvider(stream_id_);
  }
}

void ListenerStreamProvider::BindToTargetAndConnect(
    std::unique_ptr<Target> target) {
  CHECK(target);
  target_ = std::move(target);

  // TODO(b/531048253): Ensure `target` still points to a valid Document.

  DictationMultiplexer& multiplexer = GetMultiplexer();
  stream_id_ = multiplexer.GenerateStreamId();
  multiplexer.RegisterStreamProvider(stream_id_, this);

  context_fetcher_ = std::make_unique<DictationContextFetcher>();
  if (kSendContextAsync.Get()) {
    StartStream(std::nullopt);
    context_fetcher_->Fetch(
        *target_,
        base::BindOnce(&ListenerStreamProvider::OnAsyncContextCaptured,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    context_fetcher_->Fetch(
        *target_,
        base::BindOnce(&ListenerStreamProvider::OnStartContextCaptured,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void ListenerStreamProvider::StartStream(
    std::optional<DictationContext> result) {
  extensions::api::dictation_private::StartStreamDetails details;
  details.stream_id = stream_id_.value();

  if (result.has_value()) {
    details.context = ConvertToApiContext(std::move(*result));
  }

  base::ListValue event_args =
      extensions::api::dictation_private::OnStartStream::Create(details);

  auto event = std::make_unique<extensions::Event>(
      extensions::events::DICTATION_PRIVATE_ON_START_STREAM,
      extensions::api::dictation_private::OnStartStream::kEventName,
      std::move(event_args), browser_context_);

  needs_end_stream_ = true;
  extensions::EventRouter::Get(browser_context_)
      ->BroadcastEvent(std::move(event));
}

void ListenerStreamProvider::OnStartContextCaptured(DictationContext result) {
  StartStream(std::move(result));
}

void ListenerStreamProvider::OnAsyncContextCaptured(DictationContext result) {
  CHECK(stream_id_);

  extensions::api::dictation_private::ContextUpdateDetails details;
  details.stream_id = stream_id_.value();
  details.context = ConvertToApiContext(std::move(result));

  base::ListValue event_args =
      extensions::api::dictation_private::OnContextUpdate::Create(details);

  auto event = std::make_unique<extensions::Event>(
      extensions::events::DICTATION_PRIVATE_ON_CONTEXT_UPDATE,
      extensions::api::dictation_private::OnContextUpdate::kEventName,
      std::move(event_args), browser_context_);

  extensions::EventRouter::Get(browser_context_)
      ->BroadcastEvent(std::move(event));
}

void ListenerStreamProvider::Stop() {
  context_fetcher_.reset();

  if (!stream_id_) {
    return;
  }

  if (!needs_end_stream_) {
    return;
  }

  extensions::api::dictation_private::EndStreamDetails details;
  details.stream_id = stream_id_.value();

  base::ListValue event_args =
      extensions::api::dictation_private::OnEndStream::Create(details);

  auto event = std::make_unique<extensions::Event>(
      extensions::events::DICTATION_PRIVATE_ON_END_STREAM,
      extensions::api::dictation_private::OnEndStream::kEventName,
      std::move(event_args), browser_context_);

  needs_end_stream_ = false;
  extensions::EventRouter::Get(browser_context_)
      ->BroadcastEvent(std::move(event));
}

void ListenerStreamProvider::OnTranscriptionUpdated(const std::string& data,
                                                    bool is_final) {
  latest_transcription_ = data;
  is_final_for_testing_ = is_final;

  // TODO(b/531048253): Observe changes to the target and end the stream if the
  // element is no longer valid.

  target_->SetComposition(base::UTF8ToUTF16(data), is_final);

  if (update_callback_for_testing_) {
    update_callback_for_testing_.Run();
  }
}

void ListenerStreamProvider::OnStreamStateChanged(StreamState state) {
  // TODO(crbug.com/502587072): Assert state transitions are correct.
  StreamState old_state = state_;
  state_ = state;

  delegate_->DidUpdateStreamProviderState(*this, old_state);

  if (state == StreamState::kComplete) {
    target_->CommitComposition(base::UTF8ToUTF16(latest_transcription_));
  }

  if (update_callback_for_testing_) {
    update_callback_for_testing_.Run();
  }
}

void ListenerStreamProvider::SetOnUpdateForTesting(  // IN-TEST
    base::RepeatingClosure callback) {
  update_callback_for_testing_ = std::move(callback);
}

const std::string&
ListenerStreamProvider::GetLatestTranscriptionForTesting()  // IN-TEST
    const {
  return latest_transcription_;
}

bool ListenerStreamProvider::IsTranscriptionFinalForTesting() const {
  return is_final_for_testing_;
}

ListenerStreamProvider::StreamState ListenerStreamProvider::GetState() const {
  return state_;
}

const Target* ListenerStreamProvider::GetTarget() const {
  return target_.get();
}

base::WeakPtr<ListenerStreamProvider> ListenerStreamProvider::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

DictationMultiplexer& ListenerStreamProvider::GetMultiplexer() const {
  DictationKeyedService* service = DictationKeyedService::Get(browser_context_);
  CHECK(service);
  return service->multiplexer();
}

}  // namespace dictation
