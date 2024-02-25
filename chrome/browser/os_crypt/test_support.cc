// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/os_crypt/test_support.h"

#include "chrome/elevation_service/elevator.h"
#include "chrome/install_static/buildflags.h"
#include "chrome/install_static/install_modes.h"

namespace os_crypt {

namespace switches {

// Encrypt the data in input-filename and place the result in output-filename.
const char kAppBoundTestModeEncrypt[] = "encrypt";
// Decrypt the data in input-filename and place the result in output-filename.
const char kAppBoundTestModeDecrypt[] = "decrypt";
// The input file for encryption or decryption.
const char kAppBoundTestInputFilename[] = "input-filename";
// The output file for encryption or decryption.
const char kAppBoundTestOutputFilename[] = "output-filename";

}  // namespace switches

FakeInstallDetails::FakeInstallDetails()
    : constants_(install_static::kInstallModes[0]) {
  // AppGuid determines registry locations, so use a test one.
#if BUILDFLAG(USE_GOOGLE_UPDATE_INTEGRATION)
  constants_.app_guid = L"testguid";
#endif

  // This is the CLSID of the test interface, used if
  // kElevatorClsIdForTestingSwitch is supplied on the command line of the
  // elevation service.
  constants_.elevator_clsid = {elevation_service::kTestElevatorClsid};

  // This is the IID of the non-channel specific IElevator Interface. See
  // chrome/elevation_service/elevation_service_idl.idl.
  constants_.elevator_iid = {0xA949CB4E,
                             0xC4F9,
                             0x44C4,
                             {0xB2, 0x13, 0x6B, 0xF8, 0xAA, 0x9A, 0xC6,
                              0x9C}};  // IElevator IID and TypeLib
                                       // {A949CB4E-C4F9-44C4-B213-6BF8AA9AC69C}

  // These are used to generate the name of the service, so keep them
  // different from any real installs.
  constants_.base_app_name = L"testapp";
  constants_.base_app_id = L"testapp";

  // This is needed for shell_integration::GetDefaultBrowser which runs on
  // startup.
  constants_.browser_prog_id_prefix = L"TestHTM";
  constants_.pdf_prog_id_prefix = L"TestPDF";

  set_mode(&constants_);
  set_system_level(true);
}

}  // namespace os_crypt
