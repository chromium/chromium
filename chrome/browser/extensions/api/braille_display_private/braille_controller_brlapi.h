// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BRAILLE_DISPLAY_PRIVATE_BRAILLE_CONTROLLER_BRLAPI_H_
#define CHROME_BROWSER_EXTENSIONS_API_BRAILLE_DISPLAY_PRIVATE_BRAILLE_CONTROLLER_BRLAPI_H_

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "base/functional/callback_forward.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/api/braille_display_private/braille_controller.h"
#include "chrome/browser/extensions/api/braille_display_private/brlapi_connection.h"
#include "library_loaders/libbrlapi.h"

namespace extensions {
namespace api {
namespace braille_display_private {

// BrailleController implementation based on libbrlapi from brltty.
class BrailleControllerImpl : public BrailleController {
 public:
  static BrailleControllerImpl* GetInstance();
  BrailleControllerImpl(const BrailleControllerImpl&) = delete;
  BrailleControllerImpl& operator=(const BrailleControllerImpl&) = delete;
  std::unique_ptr<DisplayState> GetDisplayState() override;
  void WriteDots(const std::vector<uint8_t>& cells,
                 unsigned int cols,
                 unsigned int rows) override;
  void AddObserver(BrailleObserver* observer) override;
  void RemoveObserver(BrailleObserver* observer) override;

  // Use BrailleControllerImpl instead of Stub in tests.
  bool use_self_in_tests() const { return use_self_in_tests_; }

 private:
  // For the unit tests.
  friend class BrailleDisplayPrivateApiTest;
  friend class MockBrlapiConnection;

  BrailleControllerImpl();
  ~BrailleControllerImpl() override;
  void TryLoadLibBrlApi();

  using CreateBrlapiConnectionFunction =
      base::OnceCallback<std::unique_ptr<BrlapiConnection>()>;

  // For dependency injection in tests.  Sets the function used to create
  // brlapi connections.
  void SetCreateBrlapiConnectionForTesting(
      CreateBrlapiConnectionFunction callback);

  // Makes the controller try to reconnect (if disconnected) as if the brlapi
  // socket directory had changed.
  void PokeSocketDirForTesting();

  // Tries to connect and starts watching for new brlapi servers.
  // No-op if already called.
  void StartConnecting();
  void StartWatchingSocketDirOnTaskThread();
  void OnSocketDirChangedOnTaskThread(const base::FilePath& path, bool error);
  void OnSocketDirChangedOnIOThread();
  void TryToConnect();
  void ResetRetryConnectHorizon();
  void ScheduleTryToConnect();
  void Disconnect();
  std::unique_ptr<BrlapiConnection> CreateBrlapiConnection();
  void DispatchKeys();
  void DispatchKeyEvent(std::unique_ptr<KeyEvent> event);
  void DispatchOnDisplayStateChanged(std::unique_ptr<DisplayState> new_state);

  CreateBrlapiConnectionFunction create_brlapi_connection_function_;
  bool use_self_in_tests_ = false;

  // Manipulated on the IO thread.
  LibBrlapiLoader libbrlapi_loader_;
  std::unique_ptr<BrlapiConnection> connection_;
  bool started_connecting_ = false;
  bool connect_scheduled_ = false;
  base::Time retry_connect_horizon_;
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;

  // Manipulated on the UI thread.
  base::ObserverList<BrailleObserver>::Unchecked observers_;

  // Manipulated by the SequencedTaskRunner.
  base::FilePathWatcher file_path_watcher_;

  // Set by tests to skip libbrlapi.so loading.
  bool skip_libbrlapi_so_load_ = false;

  friend struct base::DefaultSingletonTraits<BrailleControllerImpl>;
};

}  // namespace braille_display_private
}  // namespace api
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BRAILLE_DISPLAY_PRIVATE_BRAILLE_CONTROLLER_BRLAPI_H_
