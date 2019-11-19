// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/cups_printers_manager.h"

#include <map>
#include <utility>

#include "ash/public/cpp/network_config_service.h"
#include "base/bind.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/sequence_checker.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/printing/automatic_usb_printer_configurer.h"
#include "chrome/browser/chromeos/printing/ppd_provider_factory.h"
#include "chrome/browser/chromeos/printing/ppd_resolution_tracker.h"
#include "chrome/browser/chromeos/printing/print_servers_provider.h"
#include "chrome/browser/chromeos/printing/printer_configurer.h"
#include "chrome/browser/chromeos/printing/printer_event_tracker.h"
#include "chrome/browser/chromeos/printing/printer_event_tracker_factory.h"
#include "chrome/browser/chromeos/printing/printers_map.h"
#include "chrome/browser/chromeos/printing/server_printers_provider.h"
#include "chrome/browser/chromeos/printing/synced_printers_manager.h"
#include "chrome/browser/chromeos/printing/synced_printers_manager_factory.h"
#include "chrome/browser/chromeos/printing/usb_printer_detector.h"
#include "chrome/browser/chromeos/printing/usb_printer_notification_controller.h"
#include "chrome/browser/chromeos/printing/zeroconf_printer_detector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/device_event_log/device_event_log.h"
#include "components/policy/policy_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace {

class CupsPrintersManagerImpl
    : public CupsPrintersManager,
      public SyncedPrintersManager::Observer,
      public chromeos::network_config::mojom::CrosNetworkConfigObserver {
 public:
  // Identifiers for each of the underlying PrinterDetectors this
  // class observes.
  enum DetectorIds { kUsbDetector, kZeroconfDetector, kPrintServerDetector };

  CupsPrintersManagerImpl(
      SyncedPrintersManager* synced_printers_manager,
      std::unique_ptr<PrinterDetector> usb_detector,
      std::unique_ptr<PrinterDetector> zeroconf_detector,
      scoped_refptr<PpdProvider> ppd_provider,
      std::unique_ptr<PrinterConfigurer> printer_configurer,
      std::unique_ptr<UsbPrinterNotificationController>
          usb_notification_controller,
      std::unique_ptr<ServerPrintersProvider> server_printers_provider,
      PrinterEventTracker* event_tracker,
      PrefService* pref_service)
      : synced_printers_manager_(synced_printers_manager),
        synced_printers_manager_observer_(this),
        usb_detector_(std::move(usb_detector)),
        zeroconf_detector_(std::move(zeroconf_detector)),
        ppd_provider_(std::move(ppd_provider)),
        usb_notification_controller_(std::move(usb_notification_controller)),
        auto_usb_printer_configurer_(std::move(printer_configurer),
                                     this,
                                     usb_notification_controller_.get()),
        server_printers_provider_(std::move(server_printers_provider)),
        event_tracker_(event_tracker) {
    // Add the |auto_usb_printer_configurer_| as an observer.
    AddObserver(&auto_usb_printer_configurer_);

    ash::GetNetworkConfigService(
        remote_cros_network_config_.BindNewPipeAndPassReceiver());

    remote_cros_network_config_->AddObserver(
        cros_network_config_observer_receiver_.BindNewPipeAndPassRemote());

    // Prime the printer cache with the saved and enterprise printers.
    printers_.ReplacePrintersInClass(
        PrinterClass::kSaved, synced_printers_manager_->GetSavedPrinters());
    synced_printers_manager_observer_.Add(synced_printers_manager_);

    std::vector<Printer> enterprise_printers;
    enterprise_printers_are_ready_ =
        synced_printers_manager_->GetEnterprisePrinters(&enterprise_printers);
    printers_.ReplacePrintersInClass(PrinterClass::kEnterprise,
                                     enterprise_printers);

    // Callbacks may ensue immediately when the observer proxies are set up, so
    // these instantiations must come after everything else is initialized.
    usb_detector_->RegisterPrintersFoundCallback(
        base::BindRepeating(&CupsPrintersManagerImpl::OnPrintersFound,
                            weak_ptr_factory_.GetWeakPtr(), kUsbDetector));
    OnPrintersFound(kUsbDetector, usb_detector_->GetPrinters());

    zeroconf_detector_->RegisterPrintersFoundCallback(
        base::BindRepeating(&CupsPrintersManagerImpl::OnPrintersFound,
                            weak_ptr_factory_.GetWeakPtr(), kZeroconfDetector));
    OnPrintersFound(kZeroconfDetector, zeroconf_detector_->GetPrinters());

    server_printers_provider_->RegisterPrintersFoundCallback(
        base::BindRepeating(&CupsPrintersManagerImpl::OnPrintersUpdated,
                            weak_ptr_factory_.GetWeakPtr()));

    native_printers_allowed_.Init(prefs::kUserNativePrintersAllowed,
                                  pref_service);
    send_username_and_filename_.Init(
        prefs::kPrintingSendUsernameAndFilenameEnabled, pref_service);
  }

  ~CupsPrintersManagerImpl() override = default;

  // Public API function.
  std::vector<Printer> GetPrinters(PrinterClass printer_class) const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    if (!native_printers_allowed_.GetValue() &&
        printer_class != PrinterClass::kEnterprise) {
      // If native printers are disabled then simply return an empty vector.
      LOG(WARNING) << "Attempting to retrieve native printers when "
                      "UserNativePrintersAllowed is set to false";
      return {};
    }

    if (send_username_and_filename_.GetValue()) {
      // If |send_username_and_filename_| is set, only return printers with a
      // secure protocol over which we can send username and filename.
      return printers_.GetSecurePrinters(printer_class);
    }

    // Without user data there is not need to filter out non-enterprise or
    // insecure printers so return all the printers in |printer_class|.
    return printers_.Get(printer_class);
  }

  // Public API function.
  void SavePrinter(const Printer& printer) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    if (!native_printers_allowed_.GetValue()) {
      LOG(WARNING) << "SavePrinter() called when "
                      "UserNativePrintersAllowed is set to false";
      return;
    }
    synced_printers_manager_->UpdateSavedPrinter(printer);
    // Note that we will rebuild our lists when we get the observer
    // callback from |synced_printers_manager_|.
  }

  // Public API function.
  void RemoveSavedPrinter(const std::string& printer_id) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    auto existing = synced_printers_manager_->GetPrinter(printer_id);
    if (existing) {
      event_tracker_->RecordPrinterRemoved(*existing);
    }
    synced_printers_manager_->RemoveSavedPrinter(printer_id);
    // Note that we will rebuild our lists when we get the observer
    // callback from |synced_printers_manager_|.
  }

  // Public API function.
  void AddObserver(CupsPrintersManager::Observer* observer) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    observer_list_.AddObserver(observer);
    if (enterprise_printers_are_ready_) {
      observer->OnEnterprisePrintersInitialized();
    }
  }

  // Public API function.
  void RemoveObserver(CupsPrintersManager::Observer* observer) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    observer_list_.RemoveObserver(observer);
  }

  // Public API function.
  void PrinterInstalled(const Printer& printer,
                        bool is_automatic,
                        PrinterSetupSource source) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    if (!native_printers_allowed_.GetValue()) {
      LOG(WARNING) << "PrinterInstalled() called when "
                      "UserNativePrintersAllowed is  set to false";
      return;
    }
    MaybeRecordInstallation(printer, is_automatic, source);
    MarkPrinterInstalledWithCups(printer);
  }

  // Public API function.
  bool IsPrinterInstalled(const Printer& printer) const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    const auto found = installed_printer_fingerprints_.find(printer.id());
    if (found == installed_printer_fingerprints_.end()) {
      return false;
    }

    return found->second == PrinterConfigurer::SetupFingerprint(printer);
  }

  // Public API function.
  base::Optional<Printer> GetPrinter(const std::string& id) const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    if (!native_printers_allowed_.GetValue()) {
      LOG(WARNING) << "UserNativePrintersAllowed is disabled - only searching "
                      "enterprise printers";
      return GetEnterprisePrinter(id);
    }

    return printers_.Get(id);
  }

  // SyncedPrintersManager::Observer implementation
  void OnSavedPrintersChanged() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    ResetNearbyPrintersLists();
    printers_.ReplacePrintersInClass(
        PrinterClass::kSaved, synced_printers_manager_->GetSavedPrinters());
    RebuildDetectedLists();
    NotifyObservers({PrinterClass::kSaved});
  }

  // SyncedPrintersManager::Observer implementation
  void OnEnterprisePrintersChanged() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    std::vector<Printer> enterprise_printers;
    const bool enterprise_printers_are_ready =
        synced_printers_manager_->GetEnterprisePrinters(&enterprise_printers);
    if (enterprise_printers_are_ready) {
      printers_.ReplacePrintersInClass(PrinterClass::kEnterprise,
                                       std::move(enterprise_printers));
      if (!enterprise_printers_are_ready_) {
        enterprise_printers_are_ready_ = true;
        for (auto& observer : observer_list_) {
          observer.OnEnterprisePrintersInitialized();
        }
      }
    }
    NotifyObservers({PrinterClass::kEnterprise});
  }

  // mojom::CrosNetworkConfigObserver implementation.
  void OnActiveNetworksChanged(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks) override {
    // Clear the network detected printers when the active network changes.
    // This ensures that connecting to a new network will give us only newly
    // detected printers.
    ClearNetworkDetectedPrinters();
  }
  void OnNetworkStateChanged(
      chromeos::network_config::mojom::NetworkStatePropertiesPtr /* network */)
      override {}
  void OnNetworkStateListChanged() override {}
  void OnDeviceStateListChanged() override {}
  void OnVpnProvidersChanged() override {}
  void OnNetworkCertificatesChanged() override {}

  // Callback for PrinterDetectors.
  void OnPrintersFound(
      int detector_id,
      const std::vector<PrinterDetector::DetectedPrinter>& printers) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    switch (detector_id) {
      case kUsbDetector:
        usb_detections_ = printers;
        break;
      case kZeroconfDetector:
        zeroconf_detections_ = printers;
        break;
      case kPrintServerDetector:
        servers_detections_ = printers;
        break;
    }
    RebuildDetectedLists();
  }

  // Callback for ServerPrintersProvider.
  void OnPrintersUpdated(bool complete) {
    const std::vector<PrinterDetector::DetectedPrinter> printers =
        server_printers_provider_->GetPrinters();
    if (complete) {
      PRINTER_LOG(EVENT) << "The list of server printers has been completed. "
                         << "Number of server printers: " << printers.size();
    }
    OnPrintersFound(kPrintServerDetector, printers);
  }

 private:
  base::Optional<Printer> GetEnterprisePrinter(const std::string& id) const {
    return printers_.Get(PrinterClass::kEnterprise, id);
  }

  // TODO(baileyberro): Remove the need for this function by pushing additional
  // logic into PrintersMap. https://crbug.com/956172
  void ResetNearbyPrintersLists() {
    printers_.Clear(PrinterClass::kAutomatic);
    printers_.Clear(PrinterClass::kDiscovered);
  }

  // Notify observers on the given classes the the relevant lists have changed.
  void NotifyObservers(const std::vector<PrinterClass>& printer_classes) {
    for (auto& observer : observer_list_) {
      for (auto printer_class : printer_classes) {
        observer.OnPrintersChanged(printer_class, printers_.Get(printer_class));
      }
    }
  }

  // Look through all sources for the detected printer with the given id.
  // Return a pointer to the printer on found, null if no entry is found.
  const PrinterDetector::DetectedPrinter* FindDetectedPrinter(
      const std::string& id) const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    for (const auto* printer_list : {&usb_detections_, &zeroconf_detections_}) {
      for (const auto& detected : *printer_list) {
        if (detected.printer.id() == id) {
          return &detected;
        }
      }
    }
    return nullptr;
  }

  void MaybeRecordInstallation(const Printer& printer,
                               bool is_automatic_installation,
                               PrinterSetupSource source) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    if (synced_printers_manager_->GetPrinter(printer.id())) {
      // It's just an update, not a new installation, so don't record an event.
      return;
    }

    // For compatibility with the previous implementation, record USB printers
    // separately from other IPP printers.  Eventually we may want to shift
    // this to be split by autodetected/not autodetected instead of USB/other
    // IPP.
    if (printer.IsUsbProtocol()) {
      // Get the associated detection record if one exists.
      const auto* detected = FindDetectedPrinter(printer.id());
      // We should have the full DetectedPrinter.  We can't log the printer if
      // we don't have it.
      if (!detected) {
        LOG(WARNING) << "Failed to find USB printer " << printer.id()
                     << " for installation event logging";
        return;
      }
      // For recording purposes, this is an automatic install if the ppd
      // reference generated at detection time is the is the one we actually
      // used -- i.e. the user didn't have to change anything to obtain a ppd
      // that worked.
      PrinterEventTracker::SetupMode mode;
      if (is_automatic_installation) {
        mode = PrinterEventTracker::kAutomatic;
      } else {
        mode = PrinterEventTracker::kUser;
      }
      event_tracker_->RecordUsbPrinterInstalled(*detected, mode, source);
    } else {
      PrinterEventTracker::SetupMode mode;
      if (is_automatic_installation) {
        mode = PrinterEventTracker::kAutomatic;
      } else {
        mode = PrinterEventTracker::kUser;
      }
      event_tracker_->RecordIppPrinterInstalled(printer, mode, source);
    }
  }

  void AddDetectedList(
      const std::vector<PrinterDetector::DetectedPrinter>& detected_list) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    for (const PrinterDetector::DetectedPrinter& detected : detected_list) {
      const std::string& detected_printer_id = detected.printer.id();
      if (printers_.IsPrinterInClass(PrinterClass::kSaved,
                                     detected_printer_id)) {
        // It's already in the saved class, don't need to do anything else here.
        continue;
      }

      // Sometimes the detector can flag a printer as IPP-everywhere compatible;
      // those printers can go directly into the automatic class without further
      // processing.
      if (detected.printer.IsIppEverywhere()) {
        printers_.Insert(PrinterClass::kAutomatic, detected.printer);
        continue;
      }
      if (ppd_resolution_tracker_.IsResolutionComplete(detected_printer_id)) {
        auto printer = detected.printer;
        if (!ppd_resolution_tracker_.WasResolutionSuccessful(
                detected_printer_id)) {
          if (!printer.supports_ippusb()) {
            // We couldn't figure out this printer, so it's in the discovered
            // class.
            if (printer.IsUsbProtocol()) {
              printer.set_manufacturer(
                  ppd_resolution_tracker_.GetManufacturer(detected_printer_id));
            }
            printers_.Insert(PrinterClass::kDiscovered, printer);
            continue;
          }
          // If the detected printer supports ipp-over-usb and we could not find
          // a ppd for it, then we switch to the ippusb scheme and mark it as
          // autoconf.
          printer.set_uri(
              base::StringPrintf("ippusb://%04x_%04x/ipp/print",
                                 detected.ppd_search_data.usb_vendor_id,
                                 detected.ppd_search_data.usb_product_id));
          printer.mutable_ppd_reference()->autoconf = true;
          printers_.Insert(PrinterClass::kAutomatic, printer);
        } else {
          // We have a ppd reference, so we think we can set this up
          // automatically.
          *printer.mutable_ppd_reference() =
              ppd_resolution_tracker_.GetPpdReference(detected_printer_id);
          printers_.Insert(PrinterClass::kAutomatic, printer);
        }
      } else {
        // Didn't find an entry for this printer in the PpdReferences cache.  We
        // need to ask PpdProvider whether or not it can determine a
        // PpdReference.  If there's not already an outstanding request for one,
        // start one.  When the request comes back, we'll rerun classification
        // and then should be able to figure out where this printer belongs.
        if (!ppd_resolution_tracker_.IsResolutionPending(detected_printer_id)) {
          ppd_resolution_tracker_.MarkResolutionPending(detected_printer_id);
          ppd_provider_->ResolvePpdReference(
              detected.ppd_search_data,
              base::BindOnce(&CupsPrintersManagerImpl::ResolvePpdReferenceDone,
                             weak_ptr_factory_.GetWeakPtr(),
                             detected_printer_id));
        }
      }
    }
  }

  // Record in UMA the appropriate event with a setup attempt for a printer is
  // abandoned.
  void RecordSetupAbandoned(const Printer& printer) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    if (printer.IsUsbProtocol()) {
      const auto* detected = FindDetectedPrinter(printer.id());
      if (!detected) {
        LOG(WARNING) << "Failed to find USB printer " << printer.id()
                     << " for abandoned event logging";
        return;
      }
      event_tracker_->RecordUsbSetupAbandoned(*detected);
    } else {
      event_tracker_->RecordSetupAbandoned(printer);
    }
  }

  // Rebuild the Automatic and Discovered printers lists from the (cached) raw
  // detections.  This will also generate OnPrintersChanged events for any
  // observers observering either of the detected lists (kAutomatic and
  // kDiscovered).
  void RebuildDetectedLists() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    ResetNearbyPrintersLists();
    AddDetectedList(usb_detections_);
    AddDetectedList(zeroconf_detections_);
    AddDetectedList(servers_detections_);
    NotifyObservers({PrinterClass::kAutomatic, PrinterClass::kDiscovered});
  }

  // Callback invoked on completion of PpdProvider::ResolvePpdReference.
  void ResolvePpdReferenceDone(const std::string& printer_id,
                               PpdProvider::CallbackResultCode code,
                               const Printer::PpdReference& ref,
                               const std::string& usb_manufacturer) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    if (code == PpdProvider::SUCCESS) {
      ppd_resolution_tracker_.MarkResolutionSuccessful(printer_id, ref);
    } else {
      ppd_resolution_tracker_.MarkResolutionFailed(printer_id);
      if (!usb_manufacturer.empty()) {
        ppd_resolution_tracker_.SetManufacturer(printer_id, usb_manufacturer);
      }
    }
    RebuildDetectedLists();
  }

  // Records that |printer| has been installed in CUPS.
  void MarkPrinterInstalledWithCups(const Printer& printer) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_);
    installed_printer_fingerprints_[printer.id()] =
        PrinterConfigurer::SetupFingerprint(printer);
  }

  // Resets all network detected printer lists.
  void ClearNetworkDetectedPrinters() {
    zeroconf_detections_.clear();

    ResetNearbyPrintersLists();
  }

  SEQUENCE_CHECKER(sequence_);

  // Source lists for detected printers.
  std::vector<PrinterDetector::DetectedPrinter> usb_detections_;
  std::vector<PrinterDetector::DetectedPrinter> zeroconf_detections_;
  std::vector<PrinterDetector::DetectedPrinter> servers_detections_;

  // Not owned.
  SyncedPrintersManager* const synced_printers_manager_;
  ScopedObserver<SyncedPrintersManager, SyncedPrintersManager::Observer>
      synced_printers_manager_observer_;
  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;
  mojo::Receiver<chromeos::network_config::mojom::CrosNetworkConfigObserver>
      cros_network_config_observer_receiver_{this};

  std::unique_ptr<PrinterDetector> usb_detector_;

  std::unique_ptr<PrinterDetector> zeroconf_detector_;

  scoped_refptr<PpdProvider> ppd_provider_;

  std::unique_ptr<UsbPrinterNotificationController>
      usb_notification_controller_;

  AutomaticUsbPrinterConfigurer auto_usb_printer_configurer_;

  std::unique_ptr<ServerPrintersProvider> server_printers_provider_;

  // Not owned
  PrinterEventTracker* const event_tracker_;

  // Categorized printers.  This is indexed by PrinterClass.
  PrintersMap printers_;

  // Equals true if the list of enterprise printers and related policies
  // is initialized and configured correctly.
  bool enterprise_printers_are_ready_ = false;

  // Tracks PpdReference resolution. Also stores USB manufacturer string if
  // available.
  PpdResolutionTracker ppd_resolution_tracker_;

  // Map of printer ids to PrinterConfigurer setup fingerprints at the time
  // the printers was last installed with CUPS.
  std::map<std::string, std::string> installed_printer_fingerprints_;

  base::ObserverList<CupsPrintersManager::Observer>::Unchecked observer_list_;

  // Holds the current value of the pref |UserNativePrintersAllowed|.
  BooleanPrefMember native_printers_allowed_;

  // Holds the current value of the pref
  // |PrintingSendUsernameAndFilenameEnabled|.
  BooleanPrefMember send_username_and_filename_;

  base::WeakPtrFactory<CupsPrintersManagerImpl> weak_ptr_factory_{this};
};

}  // namespace

// static
std::unique_ptr<CupsPrintersManager> CupsPrintersManager::Create(
    Profile* profile) {
  return std::make_unique<CupsPrintersManagerImpl>(
      SyncedPrintersManagerFactory::GetInstance()->GetForBrowserContext(
          profile),
      UsbPrinterDetector::Create(), ZeroconfPrinterDetector::Create(),
      CreatePpdProvider(profile), PrinterConfigurer::Create(profile),
      UsbPrinterNotificationController::Create(profile),
      ServerPrintersProvider::Create(profile),
      PrinterEventTrackerFactory::GetInstance()->GetForBrowserContext(profile),
      profile->GetPrefs());
}

// static
std::unique_ptr<CupsPrintersManager> CupsPrintersManager::CreateForTesting(
    SyncedPrintersManager* synced_printers_manager,
    std::unique_ptr<PrinterDetector> usb_detector,
    std::unique_ptr<PrinterDetector> zeroconf_detector,
    scoped_refptr<PpdProvider> ppd_provider,
    std::unique_ptr<PrinterConfigurer> printer_configurer,
    std::unique_ptr<UsbPrinterNotificationController>
        usb_notification_controller,
    std::unique_ptr<ServerPrintersProvider> server_printers_provider,
    PrinterEventTracker* event_tracker,
    PrefService* pref_service) {
  return std::make_unique<CupsPrintersManagerImpl>(
      synced_printers_manager, std::move(usb_detector),
      std::move(zeroconf_detector), std::move(ppd_provider),
      std::move(printer_configurer), std::move(usb_notification_controller),
      std::move(server_printers_provider), event_tracker, pref_service);
}

// static
void CupsPrintersManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kUserNativePrintersAllowed, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(prefs::kPrintingSendUsernameAndFilenameEnabled,
                                false);
  PrintServersProvider::RegisterProfilePrefs(registry);
}

}  // namespace chromeos
