// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_CROSTINI_PORT_FORWARDER_H_
#define CHROME_BROWSER_ASH_CROSTINI_CROSTINI_PORT_FORWARDER_H_

#include <string>
#include <unordered_map>

#include "base/files/scoped_file.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace crostini {

extern const char kDefaultInterfaceToForward[];
extern const char kWlanInterface[];
extern const char kPortNumberKey[];
extern const char kPortProtocolKey[];
extern const char kPortInterfaceKey[];
extern const char kPortLabelKey[];
extern const char kPortVmNameKey[];
extern const char kPortContainerNameKey[];

class CrostiniPortForwarder : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when a port's active state changes.
    virtual void OnActivePortsChanged(const base::Value::List& activePorts) = 0;
    virtual void OnActiveNetworkChanged(const base::Value& interface,
                                        const base::Value& ipAddress) = 0;
  };

  enum class Protocol {
    TCP = 0,
    UDP = 1,
  };

  struct PortRuleKey {
    uint16_t port_number;
    Protocol protocol_type;
    guest_os::GuestId container_id;

    bool operator==(const PortRuleKey& other) const {
      return port_number == other.port_number &&
             protocol_type == other.protocol_type;
    }
  };

  // Helper for using PortRuleKey as key entries in std::unordered_maps.
  struct PortRuleKeyHasher {
    std::size_t operator()(const PortRuleKey& k) const {
      return ((std::hash<uint16_t>()(k.port_number) ^
               (std::hash<Protocol>()(k.protocol_type) << 1)) >>
              1);
    }
  };

  using ResultCallback = base::OnceCallback<void(bool)>;

  void AddObserver(Observer* observer) { observers_.AddObserver(observer); }

  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
  }

  // The result_callback will only be called with success=true IF all conditions
  // pass. This means a port setting has been successfully updated in the
  // iptables and the profile preference setting has also been successfully
  // updated.
  void ActivatePort(const guest_os::GuestId& container_id,
                    uint16_t port_number,
                    const Protocol& protocol_type,
                    ResultCallback result_callback);
  void AddPort(const guest_os::GuestId& container_id,
               uint16_t port_number,
               const Protocol& protocol_type,
               const std::string& label,
               ResultCallback result_callback);
  void DeactivatePort(const guest_os::GuestId& container_id,
                      uint16_t port_number,
                      const Protocol& protocol_type,
                      ResultCallback result_callback);
  void RemovePort(const guest_os::GuestId& container_id,
                  uint16_t port_number,
                  const Protocol& protocol_type,
                  ResultCallback result_callback);

  // TODO(matterchen): For the two following methods, implement callback
  // results.

  // Deactivate all ports belonging to the container_id and removes them from
  // the preferences.
  void RemoveAllPorts(const guest_os::GuestId& container_id);

  // Deactivate all active ports belonging to the container_id and set their
  // preference to inactive such that these ports will not be automatically
  // re-forwarded on re-startup. This is called on container shutdown.
  void DeactivateAllActivePorts(const guest_os::GuestId& container_id);

  base::Value::List GetActivePorts();
  base::Value::List GetActiveNetworkInfo();

  size_t GetNumberOfForwardedPortsForTesting();
  std::optional<base::Value> ReadPortPreferenceForTesting(
      const PortRuleKey& key);
  void ActiveNetworksChanged(const std::string& interface,
                             const std::string& ip_address);

  explicit CrostiniPortForwarder(Profile* profile);

  CrostiniPortForwarder(const CrostiniPortForwarder&) = delete;
  CrostiniPortForwarder& operator=(const CrostiniPortForwarder&) = delete;

  ~CrostiniPortForwarder() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(CrostiniPortForwarderTest,
                           TryActivatePortPermissionBrokerClientFail);
  FRIEND_TEST_ALL_PREFIXES(CrostiniPortForwarderTest, GetActivePortsForUI);

  void SignalActivePortsChanged();
  bool MatchPortRuleDict(const base::Value& dict, const PortRuleKey& key);
  bool MatchPortRuleContainerId(const base::Value& dict,
                                const guest_os::GuestId& container_id);
  void AddNewPortPreference(const PortRuleKey& key, const std::string& label);
  bool RemovePortPreference(const PortRuleKey& key);
  std::optional<base::Value> ReadPortPreference(const PortRuleKey& key);

  void OnActivatePortCompleted(ResultCallback result_callback,
                               PortRuleKey key,
                               bool success);
  void OnRemoveOrDeactivatePortCompleted(ResultCallback result_callback,
                                         PortRuleKey key,
                                         bool success);
  void TryDeactivatePort(const PortRuleKey& key,
                         const guest_os::GuestId& container_id,
                         base::OnceCallback<void(bool)> result_callback);
  void TryActivatePort(const PortRuleKey& key,
                       const guest_os::GuestId& container_id,
                       base::OnceCallback<void(bool)> result_callback);
  void UpdateActivePortInterfaces();

  // For each port rule (protocol, port, interface), keep track of the fd which
  // requested it so we can release it on removal / deactivate.
  std::unordered_map<PortRuleKey, base::ScopedFD, PortRuleKeyHasher>
      forwarded_ports_;

  // Current interface to forward ports on.
  std::string current_interface_;
  std::string ip_address_;

  base::ObserverList<Observer> observers_;

  raw_ptr<Profile> profile_;

  base::WeakPtrFactory<CrostiniPortForwarder> weak_ptr_factory_{this};

};  // class

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_CROSTINI_PORT_FORWARDER_H_
