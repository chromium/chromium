// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/smart_card/smart_card_reader_tracker_impl.h"

#include "base/time/time.h"

namespace {

constexpr base::TimeDelta kStatusChangeTimeout = base::Minutes(5);

template <class T>
bool CopyIfChanged(T& target, const T& source) {
  if (target != source) {
    target = source;
    return true;
  }
  return false;
}

template <class T>
std::unordered_set<T> ToUnorderedSet(const std::vector<T>& v) {
  return std::unordered_set<T>(v.begin(), v.end());
}

// Wrapper to handle the PC/SC nuisance that "no readers" is expressed as an
// error instead of an empty list.
std::vector<std::string> GetReadersFromResult(
    device::mojom::SmartCardListReadersResultPtr& result) {
  if (result->is_error()) {
    CHECK_EQ(result->get_error(),
             device::mojom::SmartCardError::kNoReadersAvailable);
    return std::vector<std::string>();
  }

  return result->get_readers();
}

}  // namespace

const base::TimeDelta SmartCardReaderTrackerImpl::kMinRefreshInterval =
    base::Seconds(3);

////////////////////////////////////////////////////////////////////////////////
// SmartCardReaderTrackerImpl::State

// Represents a state in the `SmartCardReaderTracker` state machine.
// It defines how the `SmartCardReaderTracker` reacts to calls to its public
// methods and to its callbacks.
class SmartCardReaderTrackerImpl::State {
 public:
  explicit State(base::WeakPtr<SmartCardReaderTrackerImpl> tracker)
      : tracker_(tracker) {}
  virtual ~State() = default;
  virtual std::string ToString() const = 0;
  // Called when SmartCardReaderTracker enters this state.
  // This will be the first method to be called after the constructor.
  virtual void Enter() {}

  // Called when SmartCardReaderTracker::Start() is called while
  // SmartCardReaderTracker is in this state.
  virtual void Start() {}

  // Called when SmartCardReaderTracker::Stop() is called while
  // SmartCardReaderTracker is in this state.
  virtual void Stop() {}

 protected:
  base::WeakPtr<SmartCardReaderTrackerImpl> tracker_;
};

////////////////////////////////////////////////////////////////////////////////
// SmartCardReaderTrackerImpl::Uninitialized declaration

// Initial state of `SmartCardReaderTracker`.
class SmartCardReaderTrackerImpl::Uninitialized
    : public SmartCardReaderTrackerImpl::State {
 public:
  explicit Uninitialized(base::WeakPtr<SmartCardReaderTrackerImpl> tracker)
      : State(tracker) {}
  std::string ToString() const override;
  void Start() override;
};

////////////////////////////////////////////////////////////////////////////////
// SmartCardReaderTrackerImpl::WaitReadersList declaration

// `SmartCardReaderTracker` called `ListReaders` and is waiting for its result.
class SmartCardReaderTrackerImpl::WaitReadersList
    : public SmartCardReaderTrackerImpl::State {
 public:
  WaitReadersList(base::WeakPtr<SmartCardReaderTrackerImpl> tracker,
                  mojo::Remote<device::mojom::SmartCardContext> context);
  WaitReadersList(
      base::WeakPtr<SmartCardReaderTrackerImpl> tracker,
      mojo::PendingRemote<device::mojom::SmartCardContext> pending_context);
  ~WaitReadersList() override;

  std::string ToString() const override;
  void Enter() override;

 private:
  void OnListReadersDone(device::mojom::SmartCardListReadersResultPtr result);
  void RemoveAbsentReadersFromTracker(
      const std::vector<std::string>& current_readers);
  std::vector<std::string> IdentifyNewReaders(
      const std::vector<std::string>& current_readers);

  mojo::Remote<device::mojom::SmartCardContext> context_;
  base::WeakPtrFactory<SmartCardReaderTrackerImpl::WaitReadersList>
      weak_ptr_factory_{this};
};

////////////////////////////////////////////////////////////////////////////////
// SmartCardReaderTrackerImpl::Tracking

// Main state of `SmartCardReaderTracker`.
//
// `SmartCardReaderTrackerImpl::readers_` has an up to date state of all
// available smart card readers.
//
// It has a pending `SmartCardContext::GetStatusChange`request which returns
// once a change takes place.
class SmartCardReaderTrackerImpl::Tracking
    : public SmartCardReaderTrackerImpl::State {
 public:
  Tracking(base::WeakPtr<SmartCardReaderTrackerImpl> tracker,
           mojo::Remote<device::mojom::SmartCardContext> context)
      : State(tracker),
        context_(std::move(context)),
        last_start_time_(base::Time::Now()) {}

  std::string ToString() const override { return "Tracking"; }

  void Enter() override {
    CHECK(tracker_->CanTrack());

    std::vector<device::mojom::SmartCardReaderStateInPtr> reader_states;

    // Get notified when a known, existing, reader changes its state or is
    // removed from the system.
    for (auto const& [name, info] : tracker_->readers_) {
      reader_states.push_back(device::mojom::SmartCardReaderStateIn::New(
          name, CurrentStateFlagsFromInfo(info), info.event_count));
    }

    // Instead of waiting indefinitely until anything changes, wait
    // for some time and then expect to receive a timeout from PC/SC.
    // Useful as a way of telling whether the PC/SC backend is still "alive".
    context_->GetStatusChange(
        kStatusChangeTimeout, std::move(reader_states),
        base::BindOnce(
            &SmartCardReaderTrackerImpl::Tracking::OnGetStatusChangeDone,
            weak_ptr_factory_.GetWeakPtr()));
  }

  void Start() override {
    base::TimeDelta elapsed = base::Time::Now() - last_start_time_;

    if (elapsed < kMinRefreshInterval) {
      CHECK(!cancelled_);
      tracker_->FulfillRequests();
      return;
    }

    CancelGetStatusChange();
  }

  void Stop() override { CancelGetStatusChange(); }

 private:
  // Cancels the outstanding GetStatusChange() request.
  void CancelGetStatusChange() {
    if (!cancelled_) {
      context_->Cancel(
          base::BindOnce(&SmartCardReaderTrackerImpl::Tracking::OnCancelDone,
                         weak_ptr_factory_.GetWeakPtr()));

      cancelled_ = true;
    }
  }

  void OnGetStatusChangeDone(
      device::mojom::SmartCardStatusChangeResultPtr result) {
    if (result->is_error()) {
      const device::mojom::SmartCardError error = result->get_error();
      if (error == device::mojom::SmartCardError::kCancelled ||
          error == device::mojom::SmartCardError::kTimeout) {
        TrackChangesOrGiveUp();
      } else {
        tracker_->observer_list_.NotifyError(result->get_error());
        tracker_->ChangeState(std::make_unique<Uninitialized>(tracker_));
      }
      return;
    }

    tracker_->UpdateCache(result->get_reader_states());
    TrackChangesOrGiveUp();
  }

  void OnCancelDone(device::mojom::SmartCardResultPtr result) {
    CHECK(cancelled_);
    if (result->is_success()) {
      // Nothing to do here. OnGetStatusChangeDone will get called with
      // a device::mojom::SmartCardError::kCancelled.
      return;
    }
    // Cancelation has failed.
    // We have no other option than to use the cache.
    tracker_->FulfillRequests();
  }

  void TrackChangesOrGiveUp() {
    if (tracker_->CanTrack()) {
      tracker_->ChangeState(
          std::make_unique<WaitReadersList>(tracker_, std::move(context_)));
    } else {
      tracker_->ChangeState(std::make_unique<Uninitialized>(tracker_));
    }
  }

  mojo::Remote<device::mojom::SmartCardContext> context_;
  base::Time last_start_time_;
  bool cancelled_{false};
  base::WeakPtrFactory<SmartCardReaderTrackerImpl::Tracking> weak_ptr_factory_{
      this};
};

////////////////////////////////////////////////////////////////////////////////
// SmartCardReaderTrackerImpl::WaitInitialReaderStatus

// It's waiting for a `SmartCardContext::GetStatusChange` request to return
// with information on the available, but unknown to the tracker, smart card
// readers.
class SmartCardReaderTrackerImpl::WaitInitialReaderStatus
    : public SmartCardReaderTrackerImpl::State {
 public:
  explicit WaitInitialReaderStatus(
      base::WeakPtr<SmartCardReaderTrackerImpl> tracker,
      mojo::Remote<device::mojom::SmartCardContext> context,
      std::vector<std::string>& new_readers)
      : State(tracker),
        context_(std::move(context)),
        new_readers_(new_readers) {}
  std::string ToString() const override { return "WaitInitialReaderStatus"; }
  void Enter() override {
    CHECK(!new_readers_.empty());

    std::vector<device::mojom::SmartCardReaderStateInPtr> reader_states;

    for (const std::string& reader_name : new_readers_) {
      CHECK_EQ(tracker_->readers_.count(reader_name), size_t(0));

      auto state = device::mojom::SmartCardReaderStateIn::New();
      state->reader = reader_name;

      state->current_state = device::mojom::SmartCardReaderStateFlags::New(
          /*unaware=*/true, false, false, false, false, false, false, false,
          false, false, false);

      reader_states.push_back(std::move(state));
    }

    context_->GetStatusChange(
        base::TimeDelta::Max(), std::move(reader_states),
        base::BindOnce(&SmartCardReaderTrackerImpl::WaitInitialReaderStatus::
                           OnGetStatusChangeDone,
                       weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  void OnGetStatusChangeDone(
      device::mojom::SmartCardStatusChangeResultPtr result) {
    if (result->is_error()) {
      tracker_->FailRequests(device::mojom::SmartCardError::kNoService);
      tracker_->observer_list_.NotifyError(result->get_error());
      tracker_->ChangeState(std::make_unique<Uninitialized>(tracker_));
      return;
    }

    tracker_->UpdateCache(result->get_reader_states());
    // The cache is now up to date and can therefore be used to fulfill
    // all pending requests.
    tracker_->FulfillRequests();

    if (tracker_->CanTrack()) {
      tracker_->ChangeState(
          std::make_unique<Tracking>(tracker_, std::move(context_)));
    } else {
      tracker_->ChangeState(std::make_unique<Uninitialized>(tracker_));
    }
  }

  mojo::Remote<device::mojom::SmartCardContext> context_;
  std::vector<std::string> new_readers_;
  base::WeakPtrFactory<SmartCardReaderTrackerImpl::WaitInitialReaderStatus>
      weak_ptr_factory_{this};
};

////////////////////////////////////////////////////////////////////////////////
// SmartCardReaderTrackerImpl::WaitReadersList implementation

SmartCardReaderTrackerImpl::WaitReadersList::WaitReadersList(
    base::WeakPtr<SmartCardReaderTrackerImpl> tracker,
    mojo::Remote<device::mojom::SmartCardContext> context)
    : State(tracker), context_(std::move(context)) {}

SmartCardReaderTrackerImpl::WaitReadersList::WaitReadersList(
    base::WeakPtr<SmartCardReaderTrackerImpl> tracker,
    mojo::PendingRemote<device::mojom::SmartCardContext> pending_context)
    : State(tracker), context_(std::move(pending_context)) {}

SmartCardReaderTrackerImpl::WaitReadersList::~WaitReadersList() = default;

std::string SmartCardReaderTrackerImpl::WaitReadersList::ToString() const {
  return "WaitReadersList";
}

void SmartCardReaderTrackerImpl::WaitReadersList::Enter() {
  context_->ListReaders(base::BindOnce(
      &SmartCardReaderTrackerImpl::WaitReadersList::OnListReadersDone,
      weak_ptr_factory_.GetWeakPtr()));
}

void SmartCardReaderTrackerImpl::WaitReadersList::OnListReadersDone(
    device::mojom::SmartCardListReadersResultPtr result) {
  if (result->is_error() &&
      result->get_error() !=
          device::mojom::SmartCardError::kNoReadersAvailable) {
    tracker_->FailRequests(result->get_error());
    tracker_->observer_list_.NotifyError(result->get_error());
    tracker_->ChangeState(std::make_unique<Uninitialized>(tracker_));
    return;
  }

  auto current_readers = GetReadersFromResult(result);

  RemoveAbsentReadersFromTracker(current_readers);

  if (current_readers.empty()) {
    // RemoveAbsentReadersFromTracker() should have emptied it.
    CHECK(tracker_->readers_.empty());

    // There are no readers to be tracked.
    tracker_->FulfillRequests();
    tracker_->ChangeState(std::make_unique<Uninitialized>(tracker_));
    return;
  }

  std::vector<std::string> new_readers = IdentifyNewReaders(current_readers);

  if (new_readers.empty()) {
    // We already know about all existing readers and their states.
    // The cache can be considered still up to date.
    tracker_->FulfillRequests();
    // And we can go straight to Tracking (skipping WaitInitialReaderStatus).
    tracker_->ChangeState(
        std::make_unique<Tracking>(tracker_, std::move(context_)));
    return;
  }

  tracker_->ChangeState(std::make_unique<WaitInitialReaderStatus>(
      tracker_, std::move(context_), new_readers));
}

void SmartCardReaderTrackerImpl::WaitReadersList::
    RemoveAbsentReadersFromTracker(
        const std::vector<std::string>& current_readers) {
  auto current_readers_set = ToUnorderedSet(current_readers);

  for (auto it = tracker_->readers_.begin(); it != tracker_->readers_.end();) {
    std::string reader_name = it->first;
    if (current_readers_set.count(reader_name) == 0) {
      it = tracker_->readers_.erase(it);
      tracker_->observer_list_.NotifyReaderRemoved(reader_name);
    } else {
      ++it;
    }
  }
}

std::vector<std::string>
SmartCardReaderTrackerImpl::WaitReadersList::IdentifyNewReaders(
    const std::vector<std::string>& current_readers) {
  std::vector<std::string> new_readers;
  for (const std::string& reader_name : current_readers) {
    if (tracker_->readers_.count(reader_name) == 0) {
      new_readers.push_back(reader_name);
    }
  }
  return new_readers;
}

////////////////////////////////////////////////////////////////////////////////
// SmartCardReaderTrackerImpl::WaitContext

// State where the `SmartCardReaderTracker` is waiting for a
// `SmartCardContextFactory::CreateContext` call to return.
class SmartCardReaderTrackerImpl::WaitContext
    : public SmartCardReaderTrackerImpl::State {
 public:
  std::string ToString() const override { return "WaitContext"; }
  explicit WaitContext(base::WeakPtr<SmartCardReaderTrackerImpl> tracker)
      : State(tracker) {}

  void Enter() override {
    tracker_->context_factory_->CreateContext(base::BindOnce(
        &SmartCardReaderTrackerImpl::WaitContext::OnEstablishContextDone,
        weak_ptr_factory_.GetWeakPtr()));
  }

 private:
  void OnEstablishContextDone(
      device::mojom::SmartCardCreateContextResultPtr result) {
    if (result->is_error()) {
      tracker_->FailRequests(result->get_error());
      tracker_->observer_list_.NotifyError(result->get_error());
      tracker_->ChangeState(std::make_unique<Uninitialized>(tracker_));
      return;
    }

    tracker_->ChangeState(std::make_unique<WaitReadersList>(
        tracker_, std::move(result->get_context())));
  }

  base::WeakPtrFactory<SmartCardReaderTrackerImpl::WaitContext>
      weak_ptr_factory_{this};
};

////////////////////////////////////////////////////////////////////////////////
// SmartCardReaderTrackerImpl::Uninitialized implementation

std::string SmartCardReaderTrackerImpl::Uninitialized::ToString() const {
  return "Uninitialized";
}

void SmartCardReaderTrackerImpl::Uninitialized::Start() {
  tracker_->ChangeState(std::make_unique<WaitContext>(tracker_));
}

////////////////////////////////////////////////////////////////////////////////
// SmartCardReaderTrackerImpl

SmartCardReaderTrackerImpl::SmartCardReaderTrackerImpl(
    mojo::PendingRemote<device::mojom::SmartCardContextFactory> context_factory)
    : context_factory_(std::move(context_factory)) {
  state_ = std::make_unique<Uninitialized>(weak_factory_.GetWeakPtr());
}

SmartCardReaderTrackerImpl::~SmartCardReaderTrackerImpl() = default;

void SmartCardReaderTrackerImpl::Start(Observer* observer,
                                       StartCallback callback) {
  observer_list_.AddObserverIfMissing(observer);
  pending_get_readers_requests_.push(std::move(callback));
  state_->Start();
}

void SmartCardReaderTrackerImpl::Stop(Observer* observer) {
  observer_list_.RemoveObserver(observer);
  if (observer_list_.empty()) {
    state_->Stop();
  }
}

void SmartCardReaderTrackerImpl::ChangeState(
    std::unique_ptr<State> next_state) {
  CHECK(next_state);

  auto current_state = std::move(state_);

  VLOG(1) << "ChangeState: " << current_state->ToString() << " -> "
          << next_state->ToString();
  state_ = std::move(next_state);
  state_->Enter();

  // This method is invoked from inside `current_state`, so we postpone
  // destruction to ensure it has a chance to finish its current method
  // without crashing because it was deleted.
  base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(
      FROM_HERE, std::move(current_state));
}

bool SmartCardReaderTrackerImpl::CanTrack() const {
  return !observer_list_.empty() && !readers_.empty();
}

void SmartCardReaderTrackerImpl::AddReader(
    const device::mojom::SmartCardReaderStateOut& state_out) {
  readers_.insert({state_out.reader, ReaderInfoFromMojoStateOut(state_out)});
  // NB: Not informing observers since there is no `OnReaderAdded` method.
  // See comment on the Observer class definition for details.
}

void SmartCardReaderTrackerImpl::AddOrUpdateReader(
    const device::mojom::SmartCardReaderStateOut& state_out) {
  auto it = readers_.find(state_out.reader);
  if (it == readers_.end()) {
    AddReader(state_out);
  } else {
    ReaderInfo& info = it->second;
    if (UpdateInfoIfChanged(info, state_out)) {
      observer_list_.NotifyReaderChanged(info);
    }
  }
}

void SmartCardReaderTrackerImpl::RemoveReader(
    const device::mojom::SmartCardReaderStateOut& state_out) {
  auto it = readers_.find(state_out.reader);
  if (it == readers_.end()) {
    return;
  }

  readers_.erase(it);
  observer_list_.NotifyReaderRemoved(state_out.reader);
}

void SmartCardReaderTrackerImpl::GetReadersFromCache(StartCallback callback) {
  std::vector<ReaderInfo> reader_list;
  reader_list.reserve(readers_.size());

  for (const auto& [_, info] : readers_) {
    reader_list.push_back(info);
  }

  std::move(callback).Run(std::move(reader_list));
}

void SmartCardReaderTrackerImpl::UpdateCache(
    const std::vector<device::mojom::SmartCardReaderStateOutPtr>&
        reader_states) {
  for (const auto& reader_state : reader_states) {
    if (reader_state->event_state->unknown ||
        reader_state->event_state->ignore ||
        reader_state->event_state->unaware) {
      RemoveReader(*reader_state.get());
    } else {
      AddOrUpdateReader(*reader_state.get());
    }
  }
}

void SmartCardReaderTrackerImpl::FulfillRequests() {
  while (!pending_get_readers_requests_.empty()) {
    GetReadersFromCache(std::move(pending_get_readers_requests_.front()));
    pending_get_readers_requests_.pop();
  }
}

void SmartCardReaderTrackerImpl::FailRequests(
    device::mojom::SmartCardError error) {
  while (!pending_get_readers_requests_.empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(pending_get_readers_requests_.front()),
            std::optional<std::vector<SmartCardReaderTracker::ReaderInfo>>()));

    pending_get_readers_requests_.pop();
  }
}

// static
SmartCardReaderTracker::ReaderInfo
SmartCardReaderTrackerImpl::ReaderInfoFromMojoStateOut(
    const device::mojom::SmartCardReaderStateOut& state_out) {
  ReaderInfo info;
  info.name = state_out.reader;
  CopyStateFlagsIfChanged(info, *state_out.event_state.get());
  info.event_count = state_out.event_count;
  info.answer_to_reset = state_out.answer_to_reset;
  return info;
}

// static
bool SmartCardReaderTrackerImpl::CopyStateFlagsIfChanged(
    ReaderInfo& info,
    const device::mojom::SmartCardReaderStateFlags& state_flags) {
  bool changed = false;

  changed |= CopyIfChanged(info.unavailable, state_flags.unavailable);
  changed |= CopyIfChanged(info.empty, state_flags.empty);
  changed |= CopyIfChanged(info.present, state_flags.present);
  changed |= CopyIfChanged(info.exclusive, state_flags.exclusive);
  changed |= CopyIfChanged(info.inuse, state_flags.inuse);
  changed |= CopyIfChanged(info.mute, state_flags.mute);
  changed |= CopyIfChanged(info.unpowered, state_flags.unpowered);

  return changed;
}

// static
bool SmartCardReaderTrackerImpl::UpdateInfoIfChanged(
    ReaderInfo& info,
    const device::mojom::SmartCardReaderStateOut& state_out) {
  CHECK_EQ(state_out.reader, info.name);
  bool changed = false;

  changed |= CopyStateFlagsIfChanged(info, *state_out.event_state.get());
  changed |= CopyIfChanged(info.event_count, state_out.event_count);
  changed |= CopyIfChanged(info.answer_to_reset, state_out.answer_to_reset);

  return changed;
}

// static
device::mojom::SmartCardReaderStateFlagsPtr
SmartCardReaderTrackerImpl::CurrentStateFlagsFromInfo(const ReaderInfo& info) {
  auto flags = device::mojom::SmartCardReaderStateFlags::New();

  flags->unavailable = info.unavailable;
  flags->empty = info.empty;
  flags->present = info.present;
  flags->exclusive = info.exclusive;
  flags->inuse = info.inuse;
  flags->mute = info.mute;
  flags->unpowered = info.unpowered;

  return flags;
}
