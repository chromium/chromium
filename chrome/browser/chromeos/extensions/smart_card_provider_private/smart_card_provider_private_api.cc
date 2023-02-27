// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/smart_card_provider_private/smart_card_provider_private_api.h"

#include "base/notreached.h"

namespace extensions {

SmartCardProviderPrivateReportEstablishContextResultFunction::
    ~SmartCardProviderPrivateReportEstablishContextResultFunction() = default;
SmartCardProviderPrivateReportEstablishContextResultFunction::ResponseAction
SmartCardProviderPrivateReportEstablishContextResultFunction::Run() {
  // TODO(crbug.com/1386175): Implement
  NOTIMPLEMENTED();
  return RespondNow(NoArguments());
}

SmartCardProviderPrivateReportReleaseContextResultFunction::
    ~SmartCardProviderPrivateReportReleaseContextResultFunction() = default;
SmartCardProviderPrivateReportEstablishContextResultFunction::ResponseAction
SmartCardProviderPrivateReportReleaseContextResultFunction::Run() {
  // TODO(crbug.com/1386175): Implement
  NOTIMPLEMENTED();
  return RespondNow(NoArguments());
}

SmartCardProviderPrivateReportListReadersResultFunction::
    ~SmartCardProviderPrivateReportListReadersResultFunction() = default;
SmartCardProviderPrivateReportEstablishContextResultFunction::ResponseAction
SmartCardProviderPrivateReportListReadersResultFunction::Run() {
  // TODO(crbug.com/1386175): Implement
  NOTIMPLEMENTED();
  return RespondNow(NoArguments());
}

SmartCardProviderPrivateReportGetStatusChangeResultFunction::
    ~SmartCardProviderPrivateReportGetStatusChangeResultFunction() = default;
SmartCardProviderPrivateReportEstablishContextResultFunction::ResponseAction
SmartCardProviderPrivateReportGetStatusChangeResultFunction::Run() {
  // TODO(crbug.com/1386175): Implement
  NOTIMPLEMENTED();
  return RespondNow(NoArguments());
}

}  // namespace extensions
