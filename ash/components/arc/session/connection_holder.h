// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_SESSION_CONNECTION_HOLDER_H_
#define ASH_COMPONENTS_ARC_SESSION_CONNECTION_HOLDER_H_

#include <memory>
#include <type_traits>
#include <utility>

#include "ash/components/arc/session/connection_notifier.h"
#include "ash/components/arc/session/connection_observer.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/bindings/receiver.h"

// A macro to call
// ConnectionHolder<T>::GetInstanceForVersionDoNotCallDirectly(). In order to
// avoid exposing method names from within the Mojo receivers, we will rely on
// stringification and the fact that the method min versions have a consistent
// naming scheme.
#define ARC_GET_INSTANCE_FOR_METHOD(holder, method_name)        \
  (holder)->GetInstanceForVersionDoNotCallDirectly(             \
      std::remove_pointer<decltype(                             \
          holder)>::type::Instance::k##method_name##MinVersion, \
      #method_name)

namespace arc {

namespace internal {

// Small helper to implement HasInit<> trait below.
// Note that only Check() declaration is needed.
// - If InstanceType::Init exists, Check(InstanceType()) would return
//   std::true_type, because 1) the template param is successfully substituted,
//   and 2) Check(...) is weaker than the template's so there is no overload
//   conflict.
// - If not, Check(InstanceType()) would return std::false_type, because
//   template param substitution is failed, but it won't be compile error
//   thanks to SFINAE, thus Check(...) is the only candidate.
struct HasInitImpl {
  template <typename InstanceType>
  static auto Check(InstanceType* v)
      -> decltype(&InstanceType::Init, std::true_type());
  static std::false_type Check(...);
};

// Type trait to return whether InstanceType has Init() or not.
template <typename InstanceType>
using HasInit =
    decltype(HasInitImpl::Check(static_cast<InstanceType*>(nullptr)));

// Full duplex Mojo connection holder implementation.
// InstanceType and HostType are Mojo interface types (arc::mojom::XxxInstance,
// and arc::mojom::XxxHost respectively).
template <typename InstanceType, typename HostType>
class ConnectionHolderImpl {
 public:
  explicit ConnectionHolderImpl(ConnectionNotifier* connection_notifier)
      : connection_notifier_(connection_notifier) {}

  ConnectionHolderImpl(const ConnectionHolderImpl&) = delete;
  ConnectionHolderImpl& operator=(const ConnectionHolderImpl&) = delete;

  InstanceType* instance() { return IsConnected() ? instance_.get() : nullptr; }
  uint32_t instance_version() const {
    return IsConnected() ? instance_version_ : 0;
  }

  // Returns true if |receiver_| is set.
  bool IsConnected() const { return receiver_.get(); }

  // Sets (or resets if |host| is nullptr) the host.
  void SetHost(HostType* host) {
    // Some tests overwrite host, now. It is safe iff the |instance_| is
    // not yet set.
    // TODO(hidehiko): Make check more strict.
    DCHECK(host == nullptr || host_ == nullptr || instance_ == nullptr);

    if (host_ == host)
      return;

    host_ = host;
    OnChanged();
  }

  // Sets the instance.
  void SetInstance(InstanceType* instance,
                   uint32_t version = InstanceType::version_) {
    DCHECK(instance);
    DCHECK(instance_ != instance);

    instance_ = instance;
    instance_version_ = version;
    OnChanged();
  }

  // Resets the instance if it matches |instance|.
  void CloseInstance(InstanceType* instance) {
    DCHECK(instance);

    if (instance != instance_) {
      DVLOG(1) << "Dropping request to close a stale instance";
      return;
    }

    instance_ = nullptr;
    instance_version_ = 0;
    OnChanged();
  }

 private:
  // Called when |instance_| or |host_| are updated.
  void OnChanged() {
    // Cancel any in-flight connection requests. This also prevents Observers
    // from being notified of a spurious OnConnectionClosed() before an
    // OnConnectionReady() event.
    weak_ptr_factory_.InvalidateWeakPtrs();
    if (receiver_.get()) {
      // Regardless of what has changed, the old connection is now stale. Reset
      // the current receiver and notify any listeners. Since |receiver_| is set
      // just before the OnConnectionReady() event, we never notify observers of
      // OnConnectionClosed() without seeing the former event first.
      if (instance_ && host_)
        LOG(ERROR) << "Unbinding instance of a stale connection";
      OnConnectionClosed();
    }
    if (!instance_ || !host_)
      return;
    // When both the instance and host are ready, start connection.
    // TODO(crbug.com/40532557): Fix the race issue.
    auto receiver = std::make_unique<mojo::Receiver<HostType>>(host_);
    mojo::PendingRemote<HostType> host_proxy;
    receiver->Bind(host_proxy.InitWithNewPipeAndPassReceiver());
    instance_->Init(
        std::move(host_proxy),
        base::BindOnce(&ConnectionHolderImpl::OnConnectionReady,
                       weak_ptr_factory_.GetWeakPtr(), std::move(receiver)));
  }

  // Resets the receiver and notifies all the observers that the connection is
  // closed.
  void OnConnectionClosed() {
    DCHECK(receiver_);
    receiver_.reset();
    connection_notifier_->NotifyConnectionClosed();
  }

  // Notifies all the observers that the connection is ready.
  void OnConnectionReady(std::unique_ptr<mojo::Receiver<HostType>> receiver) {
    DCHECK(!receiver_);
    // Now that we can finally commit to this connection and will deliver the
    // OnConnectionReady() event, set the connection error handler to notify
    // Observers of connection closures.
    // Note: because the callback will be destroyed with |receiver_|,
    // base::Unretained() can be safely used.
    receiver->set_disconnect_handler(base::BindOnce(
        &ConnectionHolderImpl::OnConnectionClosed, base::Unretained(this)));

    receiver_ = std::move(receiver);
    connection_notifier_->NotifyConnectionReady();
  }

  // This class does not have ownership. The pointers should be managed by the
  // caller.
  const raw_ptr<ConnectionNotifier> connection_notifier_;
  raw_ptr<InstanceType, DanglingUntriaged> instance_ = nullptr;
  uint32_t instance_version_ = 0;
  raw_ptr<HostType, DanglingUntriaged> host_ = nullptr;

  // Created when both |instance_| and |host_| ptr are set.
  std::unique_ptr<mojo::Receiver<HostType>> receiver_;

  base::WeakPtrFactory<ConnectionHolderImpl> weak_ptr_factory_{this};
};

// Single direction Mojo connection holder implementation.
// (HostType == void means single direction).
// InstanceType should be Mojo interface type (arc::mojom::XxxInstance).
template <typename InstanceType>
class ConnectionHolderImpl<InstanceType, void> {
 public:
  // InstanceType must not have Init() method, which should be for a
  // full-duplex connection.
  static_assert(!HasInit<InstanceType>::value,
                "Full duplex ConnectionHolderImpl should be used instead");

  explicit ConnectionHolderImpl(ConnectionNotifier* connection_notifier)
      : connection_notifier_(connection_notifier) {}

  ConnectionHolderImpl(const ConnectionHolderImpl&) = delete;
  ConnectionHolderImpl& operator=(const ConnectionHolderImpl&) = delete;

  InstanceType* instance() { return instance_; }
  uint32_t instance_version() const { return instance_version_; }

  // For single direction connection, when Instance proxy is obtained,
  // it means connected.
  bool IsConnected() const { return instance_; }

  void SetHost(void* unused) {
    static_assert(!sizeof(*this),
                  "ConnectionHolder::SetHost for single direction connection "
                  "is called unexpectedly.");
    NOTREACHED();
  }

  // Sets the instance.
  void SetInstance(InstanceType* instance,
                   uint32_t version = InstanceType::version_) {
    DCHECK(instance);
    DCHECK(instance_ != instance);

    instance_ = instance;
    instance_version_ = version;

    // There is nothing more than setting in this case. Notify observers.
    connection_notifier_->NotifyConnectionReady();
  }

  // Resets the instance if it matches |instance|.
  void CloseInstance(InstanceType* instance) {
    DCHECK(instance);

    if (instance != instance_) {
      DVLOG(1) << "Dropping request to close a stale instance";
      return;
    }

    instance_ = nullptr;
    instance_version_ = 0;
    connection_notifier_->NotifyConnectionClosed();
  }

 private:
  // This class does not have ownership. The pointers should be managed by the
  // caller.
  const raw_ptr<ConnectionNotifier> connection_notifier_;
  raw_ptr<InstanceType, DanglingUntriaged> instance_ = nullptr;
  uint32_t instance_version_ = 0;
};

}  // namespace internal

// Holds a Mojo connection. This also allows for listening for state changes
// for the particular connection.
// InstanceType and HostType should be Mojo interface type
// (arc::mojom::XxxInstance and arc::mojom::XxxHost respectively).
// If HostType is void, it is single direction Mojo connection, so it only
// looks at Instance pointer.
// Otherwise, it is full duplex Mojo connection. When both Instance and Host
// pointers are set, it calls Instance::Init() method to pass Host pointer
// to the ARC container.
template <typename InstanceType, typename HostType = void>
class ConnectionHolder {
 public:
  using Observer = ConnectionObserver<InstanceType>;
  using Instance = InstanceType;

  ConnectionHolder() = default;

  ConnectionHolder(const ConnectionHolder&) = delete;
  ConnectionHolder& operator=(const ConnectionHolder&) = delete;

  // Returns instance version if instance is connected or 0 otherwise.
  // This method is not intended to be used directly. Instead, prefer to use
  // ARC_GET_INSTANCE_FOR_METHOD() with the expected version and check if it
  // is nullptr.
  uint32_t instance_version() const { return impl_.instance_version(); }

  // Returns true if the Mojo interface is ready at least for its version 0
  // interface. Use an Observer if you want to be notified when this is ready.
  // This can only be called on the thread that this class was created on.
  bool IsConnected() const { return impl_.IsConnected(); }

  // Gets the Mojo interface that's intended to call for
  // |method_name_for_logging|, but only if its reported version is at least
  // |min_version|. Returns nullptr if the connection is either not ready or
  // the instance does not have the requested version, and logs appropriately.
  // This function should not be called directly. Instead, use the
  // ARC_GET_INSTANCE_FOR_METHOD() macro.
  InstanceType* GetInstanceForVersionDoNotCallDirectly(
      uint32_t min_version,
      const char method_name_for_logging[]) {
    if (!impl_.IsConnected()) {
      VLOG(1) << "Instance " << InstanceType::Name_ << " not available.";
      return nullptr;
    }
    if (impl_.instance_version() < min_version) {
      LOG(ERROR) << "Instance for " << InstanceType::Name_
                 << "::" << method_name_for_logging
                 << " version mismatch. Expected " << min_version << " got "
                 << impl_.instance_version();
      return nullptr;
    }
    return impl_.instance();
  }

  // Adds or removes observers. This can only be called on the thread that this
  // class was created on. RemoveObserver does nothing if |observer| is not in
  // the list.
  void AddObserver(Observer* observer) {
    connection_notifier_.AddObserver(observer);
    if (impl_.IsConnected())
      observer->OnConnectionReady();
  }

  void RemoveObserver(Observer* observer) {
    connection_notifier_.RemoveObserver(observer);
  }

  // Sets |host|. This can be called in both cases; on ready, or on closed.
  // Passing nullptr to |host| means closing.
  // This must not be called if HostType is void (i.e. single direciton
  // connection).
  void SetHost(HostType* host) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    impl_.SetHost(host);
  }

  // Sets |instance| with |version|.
  void SetInstance(InstanceType* instance,
                   uint32_t version = InstanceType::Version_) {
    DCHECK(instance);
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    impl_.SetInstance(instance, version);
  }

  // Closes |instance|, if it matches against the current instance. Otherwise,
  // it is a no-op.
  void CloseInstance(InstanceType* instance) {
    DCHECK(instance);
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    impl_.CloseInstance(instance);
  }

 private:
  THREAD_CHECKER(thread_checker_);
  internal::ConnectionNotifier connection_notifier_;
  internal::ConnectionHolderImpl<InstanceType, HostType> impl_{
      &connection_notifier_};
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_SESSION_CONNECTION_HOLDER_H_
