// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/braille_display_private/braille_controller_brlapi.h"

#include <stddef.h>
#include <stdint.h>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/api/braille_display_private/brlapi_connection.h"
#include "chrome/browser/extensions/api/braille_display_private/brlapi_keycode_map.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {
using content::BrowserThread;
namespace api {
namespace braille_display_private {

namespace {

// Delay between detecting a directory update and trying to connect
// to the brlapi.
constexpr base::TimeDelta kConnectionDelay =
    base::TimeDelta::FromMilliseconds(500);

// How long to periodically retry connecting after a brltty restart.
// Some displays are slow to connect.
constexpr base::TimeDelta kConnectRetryTimeout =
    base::TimeDelta::FromSeconds(20);

}  // namespace

BrailleController::BrailleController() {
}

BrailleController::~BrailleController() {
}

// static
BrailleController* BrailleController::GetInstance() {
  return BrailleControllerImpl::GetInstance();
}

// static
BrailleControllerImpl* BrailleControllerImpl::GetInstance() {
  return base::Singleton<
      BrailleControllerImpl,
      base::LeakySingletonTraits<BrailleControllerImpl>>::get();
}

BrailleControllerImpl::BrailleControllerImpl()
    : started_connecting_(false),
      connect_scheduled_(false) {
  create_brlapi_connection_function_ = base::Bind(
      &BrailleControllerImpl::CreateBrlapiConnection,
      base::Unretained(this));
}

BrailleControllerImpl::~BrailleControllerImpl() {
}

void BrailleControllerImpl::TryLoadLibBrlApi() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (libbrlapi_loader_.loaded())
    return;
  // These versions of libbrlapi work the same for the functions we
  // are using.  (0.6.0 adds brlapi_writeWText).
  static const char* const kSupportedVersions[] = {
      "libbrlapi.so.0.5", "libbrlapi.so.0.6", "libbrlapi.so.0.7"};
  for (size_t i = 0; i < base::size(kSupportedVersions); ++i) {
    if (libbrlapi_loader_.Load(kSupportedVersions[i]))
      return;
  }
  LOG(WARNING) << "Couldn't load libbrlapi: " << strerror(errno);
}

std::unique_ptr<DisplayState> BrailleControllerImpl::GetDisplayState() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  StartConnecting();
  std::unique_ptr<DisplayState> display_state(new DisplayState);
  if (connection_.get() && connection_->Connected()) {
    unsigned int columns = 0;
    unsigned int rows = 0;
    if (!connection_->GetDisplaySize(&columns, &rows)) {
      Disconnect();
    } else if (rows * columns > 0) {
      // rows * columns == 0 means no display present.
      display_state->available = true;
      display_state->text_column_count.reset(new int(columns));
      display_state->text_row_count.reset(new int(rows));
    }
  }
  return display_state;
}

void BrailleControllerImpl::WriteDots(const std::vector<uint8_t>& cells,
                                      unsigned int cells_cols,
                                      unsigned int cells_rows) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (connection_ && connection_->Connected()) {
    // Row count and column count of current display.
    unsigned int columns = 0;
    unsigned int rows = 0;
    if (!connection_->GetDisplaySize(&columns, &rows)) {
      Disconnect();
    }
    std::vector<unsigned char> sized_cells(rows * columns, 0);
    unsigned int row_limit = std::min(rows, cells_rows);
    unsigned int col_limit = std::min(columns, cells_cols);
    for (unsigned int row = 0; row < row_limit; row++) {
      for (unsigned int col = 0; col < col_limit; col++) {
        sized_cells[row * columns + col] = cells[row * cells_cols + col];
      }
    }

    if (!connection_->WriteDots(sized_cells))
      Disconnect();
  }
}

void BrailleControllerImpl::AddObserver(BrailleObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!base::PostTask(FROM_HERE, {BrowserThread::IO},
                      base::BindOnce(&BrailleControllerImpl::StartConnecting,
                                     base::Unretained(this)))) {
    NOTREACHED();
  }
  observers_.AddObserver(observer);
}

void BrailleControllerImpl::RemoveObserver(BrailleObserver* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observers_.RemoveObserver(observer);
}

void BrailleControllerImpl::SetCreateBrlapiConnectionForTesting(
    const CreateBrlapiConnectionFunction& function) {
  if (function.is_null()) {
    create_brlapi_connection_function_ = base::Bind(
        &BrailleControllerImpl::CreateBrlapiConnection,
        base::Unretained(this));
  } else {
    create_brlapi_connection_function_ = function;
  }
}

void BrailleControllerImpl::PokeSocketDirForTesting() {
  OnSocketDirChangedOnIOThread();
}

void BrailleControllerImpl::StartConnecting() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (started_connecting_)
    return;
  started_connecting_ = true;
  TryLoadLibBrlApi();
  if (!libbrlapi_loader_.loaded()) {
    return;
  }

  if (!sequenced_task_runner_) {
    sequenced_task_runner_ =
        base::CreateSequencedTaskRunner({base::ThreadPool(), base::MayBlock(),
                                         base::TaskPriority::USER_VISIBLE});
  }

  // Only try to connect after we've started to watch the
  // socket directory.  This is necessary to avoid a race condition
  // and because we don't retry to connect after errors that will
  // persist until there's a change to the socket directory (i.e.
  // ENOENT).
  sequenced_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&BrailleControllerImpl::StartWatchingSocketDirOnTaskThread,
                     base::Unretained(this)),
      base::BindOnce(&BrailleControllerImpl::TryToConnect,
                     base::Unretained(this)));
  ResetRetryConnectHorizon();
}

void BrailleControllerImpl::StartWatchingSocketDirOnTaskThread() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  base::FilePath brlapi_dir(BRLAPI_SOCKETPATH);
  if (!file_path_watcher_.Watch(
          brlapi_dir, false,
          base::Bind(&BrailleControllerImpl::OnSocketDirChangedOnTaskThread,
                     base::Unretained(this)))) {
    LOG(WARNING) << "Couldn't watch brlapi directory " << BRLAPI_SOCKETPATH;
  }
}

void BrailleControllerImpl::OnSocketDirChangedOnTaskThread(
    const base::FilePath& path,
    bool error) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  if (error) {
    LOG(ERROR) << "Error watching brlapi directory: " << path.value();
    return;
  }
  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&BrailleControllerImpl::OnSocketDirChangedOnIOThread,
                     base::Unretained(this)));
}

void BrailleControllerImpl::OnSocketDirChangedOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  VLOG(1) << "BrlAPI directory changed";
  // Every directory change resets the max retry time to the appropriate delay
  // into the future.
  ResetRetryConnectHorizon();
  // Try after an initial delay to give the driver a chance to connect.
  ScheduleTryToConnect();
}

void BrailleControllerImpl::TryToConnect() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(libbrlapi_loader_.loaded());
  connect_scheduled_ = false;
  if (!connection_.get())
    connection_ = create_brlapi_connection_function_.Run();
  if (connection_.get() && !connection_->Connected()) {
    VLOG(1) << "Trying to connect to brlapi";
    BrlapiConnection::ConnectResult result = connection_->Connect(base::Bind(
        &BrailleControllerImpl::DispatchKeys,
        base::Unretained(this)));
    switch (result) {
      case BrlapiConnection::CONNECT_SUCCESS:
        DispatchOnDisplayStateChanged(GetDisplayState());
        break;
      case BrlapiConnection::CONNECT_ERROR_NO_RETRY:
        break;
      case BrlapiConnection::CONNECT_ERROR_RETRY:
        ScheduleTryToConnect();
        break;
      default:
        NOTREACHED();
    }
  }
}

void BrailleControllerImpl::ResetRetryConnectHorizon() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  retry_connect_horizon_ = base::Time::Now() + kConnectRetryTimeout;
}

void BrailleControllerImpl::ScheduleTryToConnect() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Don't reschedule if there's already a connect scheduled or
  // the next attempt would fall outside of the retry limit.
  if (connect_scheduled_)
    return;
  if (base::Time::Now() + kConnectionDelay > retry_connect_horizon_) {
    VLOG(1) << "Stopping to retry to connect to brlapi";
    return;
  }
  VLOG(1) << "Scheduling connection retry to brlapi";
  connect_scheduled_ = true;
  base::PostDelayedTask(FROM_HERE, {BrowserThread::IO},
                        base::BindOnce(&BrailleControllerImpl::TryToConnect,
                                       base::Unretained(this)),
                        kConnectionDelay);
}

void BrailleControllerImpl::Disconnect() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!connection_ || !connection_->Connected())
    return;
  connection_->Disconnect();
  DispatchOnDisplayStateChanged(
      std::unique_ptr<DisplayState>(new DisplayState()));
}

std::unique_ptr<BrlapiConnection>
BrailleControllerImpl::CreateBrlapiConnection() {
  DCHECK(libbrlapi_loader_.loaded());
  return BrlapiConnection::Create(&libbrlapi_loader_);
}

void BrailleControllerImpl::DispatchKeys() {
  DCHECK(connection_.get());
  brlapi_keyCode_t code;
  while (true) {
    int result = connection_->ReadKey(&code);
    if (result < 0) {  // Error.
      brlapi_error_t* err = connection_->BrlapiError();
      if (err->brlerrno == BRLAPI_ERROR_LIBCERR && err->libcerrno == EINTR)
        continue;
      // Disconnect on other errors.
      VLOG(1) << "BrlAPI error: " << connection_->BrlapiStrError();
      Disconnect();
      return;
    } else if (result == 0) {  // No more data.
      return;
    }
    std::unique_ptr<KeyEvent> event = BrlapiKeyCodeToEvent(code);
    if (event)
      DispatchKeyEvent(std::move(event));
  }
}

void BrailleControllerImpl::DispatchKeyEvent(std::unique_ptr<KeyEvent> event) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&BrailleControllerImpl::DispatchKeyEvent,
                       base::Unretained(this), base::Passed(&event)));
    return;
  }
  VLOG(1) << "Dispatching key event: " << *event->ToValue();
  for (auto& observer : observers_)
    observer.OnBrailleKeyEvent(*event);
}

void BrailleControllerImpl::DispatchOnDisplayStateChanged(
    std::unique_ptr<DisplayState> new_state) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    if (!base::PostTask(
            FROM_HERE, {BrowserThread::UI},
            base::BindOnce(
                &BrailleControllerImpl::DispatchOnDisplayStateChanged,
                base::Unretained(this), base::Passed(&new_state)))) {
      NOTREACHED();
    }
    return;
  }
  for (auto& observer : observers_)
    observer.OnBrailleDisplayStateChanged(*new_state);
}

}  // namespace braille_display_private
}  // namespace api
}  // namespace extensions
