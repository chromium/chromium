// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/smart_card/fake_smart_card_device_service.h"
#include "base/containers/map_util.h"

namespace {
using device::mojom::SmartCardConnectResult;
using device::mojom::SmartCardContext;
using device::mojom::SmartCardCreateContextResult;
using device::mojom::SmartCardCreateContextResultPtr;
using device::mojom::SmartCardError;
using device::mojom::SmartCardListReadersResult;
using device::mojom::SmartCardReaderStateFlags;
using device::mojom::SmartCardReaderStateIn;
using device::mojom::SmartCardReaderStateInPtr;
using device::mojom::SmartCardReaderStateOut;
using device::mojom::SmartCardReaderStateOutPtr;
using device::mojom::SmartCardResult;
using device::mojom::SmartCardStatusChangeResult;
using device::mojom::SmartCardSuccess;

constexpr char kFooReader[] = "Foo Reader";
constexpr char kAcmeReader[] = "Acme Reader";

bool StateHasChanged(const SmartCardReaderStateOut& state_out,
                     const SmartCardReaderStateIn& state_in) {
  // Try the easy cases first:

  if (state_in.current_state->unaware) {
    return true;
  }

  if (state_in.current_count != state_out.event_count) {
    return true;
  }

  // And only then go for a full diff:
  const SmartCardReaderStateFlags& flags_in = *state_in.current_state;
  const SmartCardReaderStateFlags& flags_out = *state_out.event_state;
  // clang-format off
  return flags_in.unknown != flags_out.unknown ||
         flags_in.unavailable != flags_out.unavailable ||
         flags_in.empty != flags_out.empty ||
         flags_in.present != flags_out.present ||
         flags_in.exclusive != flags_out.exclusive ||
         flags_in.inuse != flags_out.inuse ||
         flags_in.mute != flags_out.mute ||
         flags_in.unpowered != flags_out.unpowered;
  // clang-format on
}
}  // anonymous namespace

struct FakeSmartCardDeviceService::ReaderState {
  bool unknown;
  bool unavailable;
  bool empty;
  bool present;
  bool exclusive;
  bool inuse;
  bool mute;
  bool unpowered;

  uint16_t event_count;
};

struct FakeSmartCardDeviceService::PendingStatusChange {
  PendingStatusChange(std::vector<SmartCardReaderStateInPtr> reader_states,
                      SmartCardContext::GetStatusChangeCallback callback)
      : reader_states(std::move(reader_states)),
        callback(std::move(callback)) {}

  ~PendingStatusChange() = default;

  std::vector<SmartCardReaderStateInPtr> reader_states;
  SmartCardContext::GetStatusChangeCallback callback;
};

FakeSmartCardDeviceService::FakeSmartCardDeviceService() {
  readers_[kFooReader] = {.empty = true};
  readers_[kAcmeReader] = {.present = true};
}

FakeSmartCardDeviceService::~FakeSmartCardDeviceService() = default;

mojo::PendingRemote<device::mojom::SmartCardContextFactory>
FakeSmartCardDeviceService::GetSmartCardContextFactory() {
  mojo::PendingRemote<device::mojom::SmartCardContextFactory> pending_remote;
  context_factory_receivers_.Add(
      this, pending_remote.InitWithNewPipeAndPassReceiver());
  return pending_remote;
}

void FakeSmartCardDeviceService::CreateContext(CreateContextCallback callback) {
  mojo::PendingRemote<SmartCardContext> context_remote;
  context_receivers_.Add(this, context_remote.InitWithNewPipeAndPassReceiver());

  std::move(callback).Run(
      SmartCardCreateContextResult::NewContext(std::move(context_remote)));
}

void FakeSmartCardDeviceService::ListReaders(ListReadersCallback callback) {
  std::vector<std::string> names;
  for (const auto& [name, state] : readers_) {
    names.push_back(name);
  }
  std::move(callback).Run(SmartCardListReadersResult::NewReaders(names));
}

void FakeSmartCardDeviceService::GetStatusChange(
    base::TimeDelta timeout,
    std::vector<device::mojom::SmartCardReaderStateInPtr> reader_states,
    GetStatusChangeCallback callback) {
  pending_status_changes_.push_back(std::make_unique<PendingStatusChange>(
      std::move(reader_states), std::move(callback)));

  TryResolvePendingStatusChanges();
}

void FakeSmartCardDeviceService::Cancel(CancelCallback callback) {
  auto pending_list = std::move(pending_status_changes_);
  for (auto& pending_change : pending_list) {
    std::move(pending_change->callback)
        .Run(SmartCardStatusChangeResult::NewError(SmartCardError::kCancelled));
  }

  std::move(callback).Run(SmartCardResult::NewSuccess(SmartCardSuccess::kOk));
}

void FakeSmartCardDeviceService::Connect(
    const std::string& reader,
    device::mojom::SmartCardShareMode share_mode,
    device::mojom::SmartCardProtocolsPtr preferred_protocols,
    ConnectCallback callback) {
  std::move(callback).Run(
      SmartCardConnectResult::NewError(SmartCardError::kUnresponsiveCard));
}

void FakeSmartCardDeviceService::TryResolvePendingStatusChanges() {
  std::erase_if(pending_status_changes_,
                [this](std::unique_ptr<PendingStatusChange>& p) {
                  return TryResolve(*p);
                });
}

bool FakeSmartCardDeviceService::TryResolve(
    PendingStatusChange& pending_status_change) {
  std::vector<device::mojom::SmartCardReaderStateOutPtr> states_out;

  bool state_changed = false;

  for (const SmartCardReaderStateInPtr& state_in :
       pending_status_change.reader_states) {
    if (state_in->current_state->ignore) {
      continue;
    }

    const ReaderState* reader_state =
        base::FindOrNull(readers_, state_in->reader);
    auto state_out = SmartCardReaderStateOut::New();

    if (reader_state) {
      FillStateOut(*state_out, *state_in, *reader_state);
      state_changed = state_out->event_state->changed;
    } else {
      // Inform that this reader is unknown.
      auto flags = SmartCardReaderStateFlags::New();
      flags->unknown = true;
      state_out->reader = state_in->reader;
      state_out->event_state = std::move(flags);
      state_changed = true;
    }

    states_out.push_back(std::move(state_out));
  }

  if (state_changed) {
    // We only finish an outstanding GetStatusChange() request if the current
    // state of the readers is different from any of requests input states.
    std::move(pending_status_change.callback)
        .Run(SmartCardStatusChangeResult::NewReaderStates(
            std::move(states_out)));
    return true;
  }

  return false;
}

// static
void FakeSmartCardDeviceService::FillStateOut(
    SmartCardReaderStateOut& state_out,
    const SmartCardReaderStateIn& state_in,
    const ReaderState& reader_state) {
  state_out.reader = state_in.reader;

  state_out.event_state = SmartCardReaderStateFlags::New();

  state_out.event_state->unknown = reader_state.unknown;
  state_out.event_state->unavailable = reader_state.unavailable;
  state_out.event_state->empty = reader_state.empty;
  state_out.event_state->present = reader_state.present;
  state_out.event_state->exclusive = reader_state.exclusive;
  state_out.event_state->inuse = reader_state.inuse;
  state_out.event_state->mute = reader_state.mute;
  state_out.event_state->unpowered = reader_state.unpowered;

  state_out.event_count = reader_state.event_count;

  state_out.event_state->changed = StateHasChanged(state_out, state_in);
}
