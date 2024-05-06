// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/common/fast_pair/fast_pair_metrics.h"

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/common/protocol.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_number_conversions.h"
#include "components/cross_device/logging/logging.h"
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/structured_metrics_client.h"

namespace {

const char kDeviceTypeHeadphones[] = "HeadphonesDeviceType";
const char kDeviceTypeSpeaker[] = "SpeakerDeviceType";
const char kDeviceTypeTrueWirelessHeadphones[] =
    "TrueWirelessHeadphonesDeviceType";
const char kDeviceTypeMouse[] = "MouseDeviceType";
const char kDeviceTypeUnspecified[] = "UnspecifiedDeviceType";

const char kNotificationTypeFastPair[] = "FastPairNotificationType";
const char kNotificationTypeFastPairOne[] = "FastPairOneNotificationType";
const char kNotificationTypeUnspecified[] = "UnspecifiedNotificationType";

// If RSSI or TxPower are unknown, we emit -129, which is out of range of
// the real return values [-128, 127].
const int kUnknownRSSI = -129;
const int kUnknownTxPower = -129;

// Error strings should be kept in sync with the strings reflected in
// device/bluetooth/bluez/bluetooth_socket_bluez.cc.
const char kAcceptFailedString[] = "Failed to accept connection.";
const char kInvalidUUIDString[] = "Invalid UUID";
const char kSocketNotListeningString[] = "Socket is not listening.";

// Top Popular peripherals and first party devices. These device
// model names should be kept in sync with the FastPairTrackedModelID
// token in //tools/metrics/histograms/metadata/bluetooth/histograms.xml.
// Devices may have multiple Model IDs associated with the same device
// (for example, each Pixel Bud Pros have different Model IDs for each different
// color) so we append '_*' to the naming for subsequent Model IDs after the
// first one.
const char kPopularPeripheral_BoatRockerz255Pro_ModelId[] = "CFF121";
const char kPopularPeripheral_BoatRockerz255Pro_Name[] = "BoatRockerz255Pro";

const char kPopularPeripheral_BoseQuietComfort35II_ModelId[] = "0100F0";
const char kPopularPeripheral_BoseQuietComfort35II_Name[] =
    "BoseQuietComfort35II";
const char kPopularPeripheral_BoseQuietComfort35II_1_ModelId[] = "0000F0";
const char kPopularPeripheral_BoseQuietComfort35II_1_Name[] =
    "BoseQuietComfort35II_1";

const char kPopularPeripheral_JBLLIVEPROTWS_ModelId[] = "461BB8";
const char kPopularPeripheral_JBLLIVEPROTWS_Name[] = "JBLLIVEPROTWS";
const char kPopularPeripheral_JBLLIVEPROTWS_1_ModelId[] = "C6936A";
const char kPopularPeripheral_JBLLIVEPROTWS_1_Name[] = "JBLLIVEPROTWS_1";
const char kPopularPeripheral_JBLLIVEPROTWS_2_ModelId[] = "F52494";
const char kPopularPeripheral_JBLLIVEPROTWS_2_Name[] = "JBLLIVEPROTWS_2";
const char kPopularPeripheral_JBLLIVEPROTWS_3_ModelId[] = "15BA5F";
const char kPopularPeripheral_JBLLIVEPROTWS_3_Name[] = "JBLLIVEPROTWS_3";
const char kPopularPeripheral_JBLLIVEPROTWS_4_ModelId[] = "56DB24";
const char kPopularPeripheral_JBLLIVEPROTWS_4_Name[] = "JBLLIVEPROTWS_4";
const char kPopularPeripheral_JBLLIVEPROTWS_5_ModelId[] = "8CB05C";
const char kPopularPeripheral_JBLLIVEPROTWS_5_Name[] = "JBLLIVEPROTWS_5";
const char kPopularPeripheral_JBLLIVEPROTWS_6_ModelId[] = "F8013A";
const char kPopularPeripheral_JBLLIVEPROTWS_6_Name[] = "JBLLIVEPROTWS_6";

const char kPopularPeripheral_JBLLIVE300TWS_ModelId[] = "718FA4";
const char kPopularPeripheral_JBLLIVE300TWS_Name[] = "JBLLIVE300TWS";
const char kPopularPeripheral_JBLLIVE300TWS_1_ModelId[] = "7C1C37";
const char kPopularPeripheral_JBLLIVE300TWS_1_Name[] = "JBLLIVE300TWS_1";
const char kPopularPeripheral_JBLLIVE300TWS_2_ModelId[] = "2A35AD";
const char kPopularPeripheral_JBLLIVE300TWS_2_Name[] = "JBLLIVE300TWS_2";

const char kPopularPeripheral_JBLLIVE400BT_ModelId[] = "F00209";
const char kPopularPeripheral_JBLLIVE400BT_Name[] = "JBLLIVE400BT";
const char kPopularPeripheral_JBLLIVE400BT_1_ModelId[] = "F0020B";
const char kPopularPeripheral_JBLLIVE400BT_1_Name[] = "JBLLIVE400BT_1";
const char kPopularPeripheral_JBLLIVE400BT_2_ModelId[] = "F0020C";
const char kPopularPeripheral_JBLLIVE400BT_2_Name[] = "JBLLIVE400BT_2";
const char kPopularPeripheral_JBLLIVE400BT_3_ModelId[] = "F0020D";
const char kPopularPeripheral_JBLLIVE400BT_3_Name[] = "JBLLIVE400BT_3";
const char kPopularPeripheral_JBLLIVE400BT_4_ModelId[] = "F0020A";
const char kPopularPeripheral_JBLLIVE400BT_4_Name[] = "JBLLIVE400BT_4";

const char kPopularPeripheral_JBLTUNE125TWS_ModelId[] = "FF1B63";
const char kPopularPeripheral_JBLTUNE125TWS_Name[] = "JBLTUNE125TWS";
const char kPopularPeripheral_JBLTUNE125TWS_1_ModelId[] = "054B2D";
const char kPopularPeripheral_JBLTUNE125TWS_1_Name[] = "JBLTUNE125TWS_1";
const char kPopularPeripheral_JBLTUNE125TWS_2_ModelId[] = "D97EBA";
const char kPopularPeripheral_JBLTUNE125TWS_2_Name[] = "JBLTUNE125TWS_2";
const char kPopularPeripheral_JBLTUNE125TWS_3_ModelId[] = "565EAA";
const char kPopularPeripheral_JBLTUNE125TWS_3_Name[] = "JBLTUNE125TWS_3";
const char kPopularPeripheral_JBLTUNE125TWS_4_ModelId[] = "E1DD91";
const char kPopularPeripheral_JBLTUNE125TWS_4_Name[] = "JBLTUNE125TWS_4";
const char kPopularPeripheral_JBLTUNE125TWS_5_ModelId[] = "BD193B";
const char kPopularPeripheral_JBLTUNE125TWS_5_Name[] = "JBLTUNE125TWS_5";

const char kPopularPeripheral_JBLTUNE130NCTWS_ModelId[] = "BDB433";
const char kPopularPeripheral_JBLTUNE130NCTWS_Name[] = "JBLTUNE130NCTWS";
const char kPopularPeripheral_JBLTUNE130NCTWS_1_ModelId[] = "1115E7";
const char kPopularPeripheral_JBLTUNE130NCTWS_1_Name[] = "JBLTUNE130NCTWS_1";
const char kPopularPeripheral_JBLTUNE130NCTWS_2_ModelId[] = "436FD1";
const char kPopularPeripheral_JBLTUNE130NCTWS_2_Name[] = "JBLTUNE130NCTWS_2";
const char kPopularPeripheral_JBLTUNE130NCTWS_3_ModelId[] = "B73DBA";
const char kPopularPeripheral_JBLTUNE130NCTWS_3_Name[] = "JBLTUNE130NCTWS_3";

const char kPopularPeripheral_JBLTUNE225TWS_ModelId[] = "5C0C84";
const char kPopularPeripheral_JBLTUNE225TWS_Name[] = "JBLTUNE225TWS";
const char kPopularPeripheral_JBLTUNE225TWS_1_ModelId[] = "FAA6C3";
const char kPopularPeripheral_JBLTUNE225TWS_1_Name[] = "JBLTUNE225TWS_1";
const char kPopularPeripheral_JBLTUNE225TWS_2_ModelId[] = "9BC64D";
const char kPopularPeripheral_JBLTUNE225TWS_2_Name[] = "JBLTUNE225TWS_2";
const char kPopularPeripheral_JBLTUNE225TWS_3_ModelId[] = "B8393A";
const char kPopularPeripheral_JBLTUNE225TWS_3_Name[] = "JBLTUNE225TWS_3";
const char kPopularPeripheral_JBLTUNE225TWS_4_ModelId[] = "5BD6C9";
const char kPopularPeripheral_JBLTUNE225TWS_4_Name[] = "JBLTUNE225TWS_4";
const char kPopularPeripheral_JBLTUNE225TWS_5_ModelId[] = "9C98DB";
const char kPopularPeripheral_JBLTUNE225TWS_5_Name[] = "JBLTUNE225TWS_5";

const char kPopularPeripheral_JBLTUNE230NCTWS_ModelId[] = "96C12E";
const char kPopularPeripheral_JBLTUNE230NCTWS_Name[] = "JBLTUNE230NCTWS";
const char kPopularPeripheral_JBLTUNE230NCTWS_1_ModelId[] = "71F20A";
const char kPopularPeripheral_JBLTUNE230NCTWS_1_Name[] = "JBLTUNE230NCTWS_1";
const char kPopularPeripheral_JBLTUNE230NCTWS_2_ModelId[] = "EB01C0";
const char kPopularPeripheral_JBLTUNE230NCTWS_2_Name[] = "JBLTUNE230NCTWS_2";
const char kPopularPeripheral_JBLTUNE230NCTWS_3_ModelId[] = "A9394A";
const char kPopularPeripheral_JBLTUNE230NCTWS_3_Name[] = "JBLTUNE230NCTWS_3";

const char kPopularPeripheral_NothingEar1_ModelId[] = "31D53D";
const char kPopularPeripheral_NothingEar1_Name[] = "NOTHINGEAR1";
const char kPopularPeripheral_NothingEar1_1_ModelId[] = "624011";
const char kPopularPeripheral_NothingEar1_1_Name[] = "NOTHINGEAR1_1";

const char kPopularPeripheral_OnePlusBuds_ModelId[] = "5F5806";
const char kPopularPeripheral_OnePlusBuds_Name[] = "OnePlusBuds";
const char kPopularPeripheral_OnePlusBuds_1_ModelId[] = "81B915";
const char kPopularPeripheral_OnePlusBuds_1_Name[] = "OnePlusBuds_1";
const char kPopularPeripheral_OnePlusBuds_2_ModelId[] = "6C73F1";
const char kPopularPeripheral_OnePlusBuds_2_Name[] = "OnePlusBuds_2";

const char kPopularPeripheral_OnePlusBudsZ_ModelId[] = "A41C91";
const char kPopularPeripheral_OnePlusBudsZ_Name[] = "OnePlusBudsZ";
const char kPopularPeripheral_OnePlusBudsZ_1_ModelId[] = "1393DE";
const char kPopularPeripheral_OnePlusBudsZ_1_Name[] = "OnePlusBudsZ_1";
const char kPopularPeripheral_OnePlusBudsZ_2_ModelId[] = "E07634";
const char kPopularPeripheral_OnePlusBudsZ_2_Name[] = "OnePlusBudsZ_2";

const char kPopularPeripheral_PixelBuds_ModelId[] = "060000";
const char kPopularPeripheral_PixelBuds_Name[] = "PixelBuds";

const char kPopularPeripheral_PixelBudsASeries_ModelId[] = "718C17";
const char kPopularPeripheral_PixelBudsASeries_Name[] = "PixelBudsASeries";
const char kPopularPeripheral_PixelBudsASeries_1_ModelId[] = "8B66AB";
const char kPopularPeripheral_PixelBudsASeries_1_Name[] = "PixelBudsASeries_1";
const char kPopularPeripheral_PixelBudsASeries_2_ModelId[] = "3E7540";
const char kPopularPeripheral_PixelBudsASeries_2_Name[] = "PixelBudsASeries_2";

const char kPopularPeripheral_PixelBudsPro_ModelId[] = "F2020E";
const char kPopularPeripheral_PixelBudsPro_Name[] = "PixelBudsPro";
const char kPopularPeripheral_PixelBudsPro_1_ModelId[] = "6EDAF7";
const char kPopularPeripheral_PixelBudsPro_1_Name[] = "PixelBudsPro_1";
const char kPopularPeripheral_PixelBudsPro_2_ModelId[] = "5A36A5";
const char kPopularPeripheral_PixelBudsPro_2_Name[] = "PixelBudsPro_2";
const char kPopularPeripheral_PixelBudsPro_3_ModelId[] = "F58DE7";
const char kPopularPeripheral_PixelBudsPro_3_Name[] = "PixelBudsPro_3";
const char kPopularPeripheral_PixelBudsPro_4_ModelId[] = "9ADB11";
const char kPopularPeripheral_PixelBudsPro_4_Name[] = "PixelBudsPro_4";

const char kPopularPeripheral_RealMeBudsAirPro_ModelId[] = "8CD10F";
const char kPopularPeripheral_RealMeBudsAirPro_Name[] = "RealMeBudsAirPro";
const char kPopularPeripheral_RealMeBudsAirPro_1_ModelId[] = "A6E1A6";
const char kPopularPeripheral_RealMeBudsAirPro_1_Name[] = "RealMeBudsAirPro_1";
const char kPopularPeripheral_RealMeBudsAirPro_2_ModelId[] = "2F208E";
const char kPopularPeripheral_RealMeBudsAirPro_2_Name[] = "RealMeBudsAirPro_2";

const char kPopularPeripheral_RealMeBudsAir2_ModelId[] = "BA5D56";
const char kPopularPeripheral_RealMeBudsAir2_Name[] = "RealMeBudsAir2";

const char kPopularPeripheral_RealMeBudsAir2Neo_ModelId[] = "0B5374";
const char kPopularPeripheral_RealMeBudsAir2Neo_Name[] = "RealMeBudsAir2Neo";

const char kPopularPeripheral_RealMeBudsQ2TWS_ModelId[] = "72C415";
const char kPopularPeripheral_RealMeBudsQ2TWS_Name[] = "RealMeBudsQ2TWS";

const char kPopularPeripheral_RealMeTechLifeBudsT100_ModelId[] = "29C992";
const char kPopularPeripheral_RealMeTechLifeBudsT100_Name[] =
    "RealMeTechLifeBudsT100";
const char kPopularPeripheral_RealMeTechLifeBudsT100_1_ModelId[] = "D5C6CE";
const char kPopularPeripheral_RealMeTechLifeBudsT100_1_Name[] =
    "RealMeTechLifeBudsT100_1";
const char kPopularPeripheral_RealMeTechLifeBudsT100_2_ModelId[] = "62E69F";
const char kPopularPeripheral_RealMeTechLifeBudsT100_2_Name[] =
    "RealMeTechLifeBudsT100_2";

const char kPopularPeripheral_SonyWF1000XM3_ModelId[] = "38C95C";
const char kPopularPeripheral_SonyWF1000XM3_Name[] = "SonyWF1000XM3";
const char kPopularPeripheral_SonyWF1000XM3_1_ModelId[] = "9C98DB";
const char kPopularPeripheral_SonyWF1000XM3_1_Name[] = "SonyWF1000XM3_1";
const char kPopularPeripheral_SonyWF1000XM3_2_ModelId[] = "3BC95C";
const char kPopularPeripheral_SonyWF1000XM3_2_Name[] = "SonyWF1000XM3_2";
const char kPopularPeripheral_SonyWF1000XM3_3_ModelId[] = "3AC95C";
const char kPopularPeripheral_SonyWF1000XM3_3_Name[] = "SonyWF1000XM3_3";
const char kPopularPeripheral_SonyWF1000XM3_4_ModelId[] = "0AC95C";
const char kPopularPeripheral_SonyWF1000XM3_4_Name[] = "SonyWF1000XM3_4";
const char kPopularPeripheral_SonyWF1000XM3_5_ModelId[] = "0DC95C";
const char kPopularPeripheral_SonyWF1000XM3_5_Name[] = "SonyWF1000XM3_5";
const char kPopularPeripheral_SonyWF1000XM3_6_ModelId[] = "0BC95C";
const char kPopularPeripheral_SonyWF1000XM3_6_Name[] = "SonyWF1000XM3_6";
const char kPopularPeripheral_SonyWF1000XM3_7_ModelId[] = "0CC95C";
const char kPopularPeripheral_SonyWF1000XM3_7_Name[] = "SonyWF1000XM3_7";

const char kPopularPeripheral_SonyWH1000XM3_ModelId[] = "0BC95C";
const char kPopularPeripheral_SonyWH1000XM3_Name[] = "SonyWH1000XM3";
const char kPopularPeripheral_SonyWH1000XM3_1_ModelId[] = "AC95C";
const char kPopularPeripheral_SonyWH1000XM3_1_Name[] = "SonyWH1000XM3_1";

const char kPopularPeripheral_SRSXB13_ModelId[] = "741594";
const char kPopularPeripheral_SRSXB13_Name[] = "SRSXB13";
const char kPopularPeripheral_SRSXB13_1_ModelId[] = "DF4B02";
const char kPopularPeripheral_SRSXB13_1_Name[] = "SRSXB13_1";
const char kPopularPeripheral_SRSXB13_2_ModelId[] = "F5CEC7";
const char kPopularPeripheral_SRSXB13_2_Name[] = "SRSXB13_2";
const char kPopularPeripheral_SRSXB13_3_ModelId[] = "FFB35B";
const char kPopularPeripheral_SRSXB13_3_Name[] = "SRSXB13_3";
const char kPopularPeripheral_SRSXB13_4_ModelId[] = "36EFA5";
const char kPopularPeripheral_SRSXB13_4_Name[] = "SRSXB13_4";
const char kPopularPeripheral_SRSXB13_5_ModelId[] = "3E6B5B";
const char kPopularPeripheral_SRSXB13_5_Name[] = "SRSXB13_5";

const char kPopularPeripheral_SRSXB23_ModelId[] = "30D222";
const char kPopularPeripheral_SRSXB23_Name[] = "SRSXB23";
const char kPopularPeripheral_SRSXB23_1_ModelId[] = "438188";
const char kPopularPeripheral_SRSXB23_1_Name[] = "SRSXB23_1";
const char kPopularPeripheral_SRSXB23_2_ModelId[] = "4A9EF6";
const char kPopularPeripheral_SRSXB23_2_Name[] = "SRSXB23_2";
const char kPopularPeripheral_SRSXB23_3_ModelId[] = "6ABCC9";
const char kPopularPeripheral_SRSXB23_3_Name[] = "SRSXB23_3";
const char kPopularPeripheral_SRSXB23_4_ModelId[] = "3414EB";
const char kPopularPeripheral_SRSXB23_4_Name[] = "SRSXB23_4";

const char kPopularPeripheral_SRSXB33_ModelId[] = "20330C";
const char kPopularPeripheral_SRSXB33_Name[] = "SRSXB33";
const char kPopularPeripheral_SRSXB33_1_ModelId[] = "91DABC";
const char kPopularPeripheral_SRSXB33_1_Name[] = "SRSXB33_1";
const char kPopularPeripheral_SRSXB33_2_ModelId[] = "E5B91B";
const char kPopularPeripheral_SRSXB33_2_Name[] = "SRSXB33_2";
const char kPopularPeripheral_SRSXB33_3_ModelId[] = "5A0DDA";
const char kPopularPeripheral_SRSXB33_3_Name[] = "SRSXB33_3";

const char kPopularPeripheral_Other_Name[] = "Other";

const std::string GetFastPairTrackedModelId(const std::string& model_id) {
  if (model_id == kPopularPeripheral_BoatRockerz255Pro_ModelId) {
    return kPopularPeripheral_BoatRockerz255Pro_Name;
  }

  if (model_id == kPopularPeripheral_BoseQuietComfort35II_ModelId) {
    return kPopularPeripheral_BoseQuietComfort35II_Name;
  }
  if (model_id == kPopularPeripheral_BoseQuietComfort35II_1_ModelId) {
    return kPopularPeripheral_BoseQuietComfort35II_1_Name;
  }

  if (model_id == kPopularPeripheral_JBLLIVEPROTWS_ModelId) {
    return kPopularPeripheral_JBLLIVEPROTWS_Name;
  }
  if (model_id == kPopularPeripheral_JBLLIVEPROTWS_1_ModelId) {
    return kPopularPeripheral_JBLLIVEPROTWS_1_Name;
  }
  if (model_id == kPopularPeripheral_JBLLIVEPROTWS_2_ModelId) {
    return kPopularPeripheral_JBLLIVEPROTWS_2_Name;
  }
  if (model_id == kPopularPeripheral_JBLLIVEPROTWS_3_ModelId) {
    return kPopularPeripheral_JBLLIVEPROTWS_3_Name;
  }
  if (model_id == kPopularPeripheral_JBLLIVEPROTWS_4_ModelId) {
    return kPopularPeripheral_JBLLIVEPROTWS_4_Name;
  }
  if (model_id == kPopularPeripheral_JBLLIVEPROTWS_5_ModelId) {
    return kPopularPeripheral_JBLLIVEPROTWS_5_Name;
  }
  if (model_id == kPopularPeripheral_JBLLIVEPROTWS_6_ModelId) {
    return kPopularPeripheral_JBLLIVEPROTWS_6_Name;
  }

  if (model_id == kPopularPeripheral_JBLLIVE300TWS_ModelId) {
    return kPopularPeripheral_JBLLIVE300TWS_Name;
  }
  if (model_id == kPopularPeripheral_JBLLIVE300TWS_1_ModelId) {
    return kPopularPeripheral_JBLLIVE300TWS_1_Name;
  }
  if (model_id == kPopularPeripheral_JBLLIVE300TWS_2_ModelId) {
    return kPopularPeripheral_JBLLIVE300TWS_2_Name;
  }

  if (model_id == kPopularPeripheral_JBLLIVE400BT_ModelId) {
    return kPopularPeripheral_JBLLIVE400BT_Name;
  }
  if (model_id == kPopularPeripheral_JBLLIVE400BT_1_ModelId) {
    return kPopularPeripheral_JBLLIVE400BT_1_Name;
  }
  if (model_id == kPopularPeripheral_JBLLIVE400BT_2_ModelId) {
    return kPopularPeripheral_JBLLIVE400BT_2_Name;
  }
  if (model_id == kPopularPeripheral_JBLLIVE400BT_3_ModelId) {
    return kPopularPeripheral_JBLLIVE400BT_3_Name;
  }
  if (model_id == kPopularPeripheral_JBLLIVE400BT_4_ModelId) {
    return kPopularPeripheral_JBLLIVE400BT_4_Name;
  }

  if (model_id == kPopularPeripheral_JBLTUNE125TWS_ModelId) {
    return kPopularPeripheral_JBLTUNE125TWS_Name;
  }
  if (model_id == kPopularPeripheral_JBLTUNE125TWS_1_ModelId) {
    return kPopularPeripheral_JBLTUNE125TWS_1_Name;
  }
  if (model_id == kPopularPeripheral_JBLTUNE125TWS_2_ModelId) {
    return kPopularPeripheral_JBLTUNE125TWS_2_Name;
  }
  if (model_id == kPopularPeripheral_JBLTUNE125TWS_3_ModelId) {
    return kPopularPeripheral_JBLTUNE125TWS_3_Name;
  }
  if (model_id == kPopularPeripheral_JBLTUNE125TWS_4_ModelId) {
    return kPopularPeripheral_JBLTUNE125TWS_4_Name;
  }
  if (model_id == kPopularPeripheral_JBLTUNE125TWS_5_ModelId) {
    return kPopularPeripheral_JBLTUNE125TWS_5_Name;
  }

  if (model_id == kPopularPeripheral_JBLTUNE130NCTWS_ModelId) {
    return kPopularPeripheral_JBLTUNE130NCTWS_Name;
  }
  if (model_id == kPopularPeripheral_JBLTUNE130NCTWS_1_ModelId) {
    return kPopularPeripheral_JBLTUNE130NCTWS_1_Name;
  }
  if (model_id == kPopularPeripheral_JBLTUNE130NCTWS_2_ModelId) {
    return kPopularPeripheral_JBLTUNE130NCTWS_2_Name;
  }
  if (model_id == kPopularPeripheral_JBLTUNE130NCTWS_3_ModelId) {
    return kPopularPeripheral_JBLTUNE130NCTWS_3_Name;
  }

  if (model_id == kPopularPeripheral_JBLTUNE225TWS_ModelId) {
    return kPopularPeripheral_JBLTUNE225TWS_Name;
  }
  if (model_id == kPopularPeripheral_JBLTUNE225TWS_1_ModelId) {
    return kPopularPeripheral_JBLTUNE225TWS_1_Name;
  }
  if (model_id == kPopularPeripheral_JBLTUNE225TWS_2_ModelId) {
    return kPopularPeripheral_JBLTUNE225TWS_2_Name;
  }
  if (model_id == kPopularPeripheral_JBLTUNE225TWS_3_ModelId) {
    return kPopularPeripheral_JBLTUNE225TWS_3_Name;
  }
  if (model_id == kPopularPeripheral_JBLTUNE225TWS_4_ModelId) {
    return kPopularPeripheral_JBLTUNE225TWS_4_Name;
  }
  if (model_id == kPopularPeripheral_JBLTUNE225TWS_5_ModelId) {
    return kPopularPeripheral_JBLTUNE225TWS_5_Name;
  }

  if (model_id == kPopularPeripheral_JBLTUNE230NCTWS_ModelId) {
    return kPopularPeripheral_JBLTUNE230NCTWS_Name;
  }
  if (model_id == kPopularPeripheral_JBLTUNE230NCTWS_1_ModelId) {
    return kPopularPeripheral_JBLTUNE230NCTWS_1_Name;
  }
  if (model_id == kPopularPeripheral_JBLTUNE230NCTWS_2_ModelId) {
    return kPopularPeripheral_JBLTUNE230NCTWS_2_Name;
  }
  if (model_id == kPopularPeripheral_JBLTUNE230NCTWS_3_ModelId) {
    return kPopularPeripheral_JBLTUNE230NCTWS_3_Name;
  }

  if (model_id == kPopularPeripheral_NothingEar1_ModelId) {
    return kPopularPeripheral_NothingEar1_Name;
  }
  if (model_id == kPopularPeripheral_NothingEar1_1_ModelId) {
    return kPopularPeripheral_NothingEar1_1_Name;
  }

  if (model_id == kPopularPeripheral_OnePlusBuds_ModelId) {
    return kPopularPeripheral_OnePlusBuds_Name;
  }
  if (model_id == kPopularPeripheral_OnePlusBuds_1_ModelId) {
    return kPopularPeripheral_OnePlusBuds_1_Name;
  }
  if (model_id == kPopularPeripheral_OnePlusBuds_2_ModelId) {
    return kPopularPeripheral_OnePlusBuds_2_Name;
  }

  if (model_id == kPopularPeripheral_OnePlusBudsZ_ModelId) {
    return kPopularPeripheral_OnePlusBudsZ_Name;
  }
  if (model_id == kPopularPeripheral_OnePlusBudsZ_1_ModelId) {
    return kPopularPeripheral_OnePlusBudsZ_1_Name;
  }
  if (model_id == kPopularPeripheral_OnePlusBudsZ_2_ModelId) {
    return kPopularPeripheral_OnePlusBudsZ_2_Name;
  }

  if (model_id == kPopularPeripheral_PixelBuds_ModelId) {
    return kPopularPeripheral_PixelBuds_Name;
  }

  if (model_id == kPopularPeripheral_PixelBudsASeries_ModelId) {
    return kPopularPeripheral_PixelBudsASeries_Name;
  }
  if (model_id == kPopularPeripheral_PixelBudsASeries_1_ModelId) {
    return kPopularPeripheral_PixelBudsASeries_1_Name;
  }
  if (model_id == kPopularPeripheral_PixelBudsASeries_2_ModelId) {
    return kPopularPeripheral_PixelBudsASeries_2_Name;
  }

  if (model_id == kPopularPeripheral_PixelBudsPro_ModelId) {
    return kPopularPeripheral_PixelBudsPro_Name;
  }
  if (model_id == kPopularPeripheral_PixelBudsPro_1_ModelId) {
    return kPopularPeripheral_PixelBudsPro_1_Name;
  }
  if (model_id == kPopularPeripheral_PixelBudsPro_2_ModelId) {
    return kPopularPeripheral_PixelBudsPro_2_Name;
  }
  if (model_id == kPopularPeripheral_PixelBudsPro_3_ModelId) {
    return kPopularPeripheral_PixelBudsPro_3_Name;
  }
  if (model_id == kPopularPeripheral_PixelBudsPro_4_ModelId) {
    return kPopularPeripheral_PixelBudsPro_4_Name;
  }

  if (model_id == kPopularPeripheral_RealMeBudsAir2_ModelId) {
    return kPopularPeripheral_RealMeBudsAir2_Name;
  }

  if (model_id == kPopularPeripheral_RealMeBudsAir2Neo_ModelId) {
    return kPopularPeripheral_RealMeBudsAir2Neo_Name;
  }

  if (model_id == kPopularPeripheral_RealMeBudsAirPro_ModelId) {
    return kPopularPeripheral_RealMeBudsAirPro_Name;
  }
  if (model_id == kPopularPeripheral_RealMeBudsAirPro_1_ModelId) {
    return kPopularPeripheral_RealMeBudsAirPro_1_Name;
  }
  if (model_id == kPopularPeripheral_RealMeBudsAirPro_2_ModelId) {
    return kPopularPeripheral_RealMeBudsAirPro_2_Name;
  }

  if (model_id == kPopularPeripheral_RealMeBudsQ2TWS_ModelId) {
    return kPopularPeripheral_RealMeBudsQ2TWS_Name;
  }

  if (model_id == kPopularPeripheral_RealMeTechLifeBudsT100_ModelId) {
    return kPopularPeripheral_RealMeTechLifeBudsT100_Name;
  }
  if (model_id == kPopularPeripheral_RealMeTechLifeBudsT100_1_ModelId) {
    return kPopularPeripheral_RealMeTechLifeBudsT100_1_Name;
  }
  if (model_id == kPopularPeripheral_RealMeTechLifeBudsT100_2_ModelId) {
    return kPopularPeripheral_RealMeTechLifeBudsT100_2_Name;
  }

  if (model_id == kPopularPeripheral_SonyWF1000XM3_ModelId) {
    return kPopularPeripheral_SonyWF1000XM3_Name;
  }
  if (model_id == kPopularPeripheral_SonyWF1000XM3_1_ModelId) {
    return kPopularPeripheral_SonyWF1000XM3_1_Name;
  }
  if (model_id == kPopularPeripheral_SonyWF1000XM3_2_ModelId) {
    return kPopularPeripheral_SonyWF1000XM3_2_Name;
  }
  if (model_id == kPopularPeripheral_SonyWF1000XM3_3_ModelId) {
    return kPopularPeripheral_SonyWF1000XM3_3_Name;
  }
  if (model_id == kPopularPeripheral_SonyWF1000XM3_4_ModelId) {
    return kPopularPeripheral_SonyWF1000XM3_4_Name;
  }
  if (model_id == kPopularPeripheral_SonyWF1000XM3_5_ModelId) {
    return kPopularPeripheral_SonyWF1000XM3_5_Name;
  }
  if (model_id == kPopularPeripheral_SonyWF1000XM3_6_ModelId) {
    return kPopularPeripheral_SonyWF1000XM3_6_Name;
  }
  if (model_id == kPopularPeripheral_SonyWF1000XM3_7_ModelId) {
    return kPopularPeripheral_SonyWF1000XM3_7_Name;
  }

  if (model_id == kPopularPeripheral_SonyWH1000XM3_ModelId) {
    return kPopularPeripheral_SonyWH1000XM3_Name;
  }
  if (model_id == kPopularPeripheral_SonyWH1000XM3_1_ModelId) {
    return kPopularPeripheral_SonyWH1000XM3_1_Name;
  }

  if (model_id == kPopularPeripheral_SRSXB33_ModelId) {
    return kPopularPeripheral_SRSXB33_Name;
  }
  if (model_id == kPopularPeripheral_SRSXB33_1_ModelId) {
    return kPopularPeripheral_SRSXB33_1_Name;
  }
  if (model_id == kPopularPeripheral_SRSXB33_2_ModelId) {
    return kPopularPeripheral_SRSXB33_2_Name;
  }
  if (model_id == kPopularPeripheral_SRSXB33_3_ModelId) {
    return kPopularPeripheral_SRSXB33_3_Name;
  }

  if (model_id == kPopularPeripheral_SRSXB23_ModelId) {
    return kPopularPeripheral_SRSXB23_Name;
  }
  if (model_id == kPopularPeripheral_SRSXB23_1_ModelId) {
    return kPopularPeripheral_SRSXB23_1_Name;
  }
  if (model_id == kPopularPeripheral_SRSXB23_2_ModelId) {
    return kPopularPeripheral_SRSXB23_2_Name;
  }
  if (model_id == kPopularPeripheral_SRSXB23_3_ModelId) {
    return kPopularPeripheral_SRSXB23_3_Name;
  }
  if (model_id == kPopularPeripheral_SRSXB23_4_ModelId) {
    return kPopularPeripheral_SRSXB23_4_Name;
  }

  if (model_id == kPopularPeripheral_SRSXB13_ModelId) {
    return kPopularPeripheral_SRSXB13_Name;
  }
  if (model_id == kPopularPeripheral_SRSXB13_1_ModelId) {
    return kPopularPeripheral_SRSXB13_1_Name;
  }
  if (model_id == kPopularPeripheral_SRSXB13_2_ModelId) {
    return kPopularPeripheral_SRSXB13_2_Name;
  }
  if (model_id == kPopularPeripheral_SRSXB13_3_ModelId) {
    return kPopularPeripheral_SRSXB13_3_Name;
  }
  if (model_id == kPopularPeripheral_SRSXB13_4_ModelId) {
    return kPopularPeripheral_SRSXB13_4_Name;
  }
  if (model_id == kPopularPeripheral_SRSXB13_5_ModelId) {
    return kPopularPeripheral_SRSXB13_5_Name;
  }

  return kPopularPeripheral_Other_Name;
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. This enum should be kept in sync
// with the BluetoothConnectToServiceError enum in
// //tools/metrics/histograms/metadata/bluetooth/enums.xml.
enum class ConnectToServiceError {
  kUnknownError = 0,
  kAcceptFailed = 1,
  kInvalidUUID = 2,
  kSocketNotListening = 3,
  kMaxValue = kSocketNotListening,
};

ConnectToServiceError GetConnectToServiceError(const std::string& error) {
  if (error == kAcceptFailedString) {
    return ConnectToServiceError::kAcceptFailed;
  }

  if (error == kInvalidUUIDString) {
    return ConnectToServiceError::kInvalidUUID;
  }

  if (error == kSocketNotListeningString) {
    return ConnectToServiceError::kSocketNotListening;
  }

  DCHECK(error != kSocketNotListeningString && error != kInvalidUUIDString &&
         error != kAcceptFailedString);
  return ConnectToServiceError::kUnknownError;
}

std::optional<std::string> GetFastPairDeviceType(
    const nearby::fastpair::Device& device_metadata) {
  // Needs to stay up to date with `DeviceType` enum in
  // ash/quick_pair/proto/enums.proto. We only expect these device
  // types because of filtering in the scanning component. Always expected to
  // be one of these values.
  if (device_metadata.device_type() ==
      nearby::fastpair::DeviceType::HEADPHONES) {
    return kDeviceTypeHeadphones;
  } else if (device_metadata.device_type() ==
             nearby::fastpair::DeviceType::SPEAKER) {
    return kDeviceTypeSpeaker;
  } else if (device_metadata.device_type() ==
             nearby::fastpair::DeviceType::TRUE_WIRELESS_HEADPHONES) {
    return kDeviceTypeTrueWirelessHeadphones;
  } else if (device_metadata.device_type() ==
             nearby::fastpair::DeviceType::MOUSE) {
    return kDeviceTypeMouse;
  } else if (device_metadata.device_type() ==
             nearby::fastpair::DeviceType::DEVICE_TYPE_UNSPECIFIED) {
    return kDeviceTypeUnspecified;
  }

  return std::nullopt;
}

std::optional<std::string> GetFastPairNotificationType(
    const nearby::fastpair::Device& device_metadata) {
  // Needs to stay up to date with `NotificationType` enum in
  // ash/quick_pair/proto/enums.proto. We only expect these notification
  // types because of filtering in the scanning component. Always expected to
  // be one of these values.
  DCHECK(device_metadata.notification_type() ==
             nearby::fastpair::NotificationType::FAST_PAIR ||
         device_metadata.notification_type() ==
             nearby::fastpair::NotificationType::FAST_PAIR_ONE ||
         device_metadata.notification_type() ==
             nearby::fastpair::NotificationType::NOTIFICATION_TYPE_UNSPECIFIED);
  if (device_metadata.notification_type() ==
      nearby::fastpair::NotificationType::FAST_PAIR) {
    return kNotificationTypeFastPair;
  } else if (device_metadata.notification_type() ==
             nearby::fastpair::NotificationType::FAST_PAIR_ONE) {
    return kNotificationTypeFastPairOne;
  } else if (device_metadata.notification_type() ==
             nearby::fastpair::NotificationType::
                 NOTIFICATION_TYPE_UNSPECIFIED) {
    return kNotificationTypeUnspecified;
  } else {
    return std::nullopt;
  }
}

const char kEngagementFlowInitialMetric[] =
    "Bluetooth.ChromeOS.FastPair.EngagementFunnel.Steps.InitialPairingProtocol";
const char kEngagementFlowSubsequentMetric[] =
    "Bluetooth.ChromeOS.FastPair.EngagementFunnel.Steps."
    "SubsequentPairingProtocol";
const char kTotalUxPairTimeInitialMetric[] =
    "Bluetooth.ChromeOS.FastPair.TotalUxPairTime.InitialPairingProtocol2";
const char kTotalUxPairTimeSubsequentMetric[] =
    "Bluetooth.ChromeOS.FastPair.TotalUxPairTime.SubsequentPairingProtocol2";
const char kRetroactiveEngagementFlowMetric[] =
    "Bluetooth.ChromeOS.FastPair.RetroactiveEngagementFunnel.Steps";
const char kPairingMethodMetric[] = "Bluetooth.ChromeOS.FastPair.PairingMethod";
const char kRetroactivePairingResultMetric[] =
    "Bluetooth.ChromeOS.FastPair.RetroactivePairing.Result";
const char kTotalGattConnectionTimeMetric[] =
    "Bluetooth.ChromeOS.FastPair.TotalGattConnectionTime";
const char kGattConnectionResult[] =
    "Bluetooth.ChromeOS.FastPair.GattConnection.Result";
const char kGattConnectionErrorMetric[] =
    "Bluetooth.ChromeOS.FastPair.GattConnection.ErrorReason";
const char kGattConnectionEffectiveSuccessRate[] =
    "Bluetooth.ChromeOS.FastPair.GattConnection.EffectiveSuccessRate";
const char kGattConnectionAttemptCount[] =
    "Bluetooth.ChromeOS.FastPair.GattConnection.AttemptCount";
const char kFastPairGattRetryFailureReason[] =
    "Bluetooth.ChromeOS.FastPair.GattConnection.RetryFailureReason";
const char kFastPairPairFailureInitialMetric[] =
    "Bluetooth.ChromeOS.FastPair.PairFailure.InitialPairingProtocol";
const char kFastPairPairFailureSubsequentMetric[] =
    "Bluetooth.ChromeOS.FastPair.PairFailure.SubsequentPairingProtocol";
const char kFastPairPairFailureRetroactiveMetric[] =
    "Bluetooth.ChromeOS.FastPair.PairFailure.RetroactivePairingProtocol";
const char kFastPairPairResultInitialMetric[] =
    "Bluetooth.ChromeOS.FastPair.Pairing.Result.InitialPairingProtocol";
const char kFastPairPairResultSubsequentMetric[] =
    "Bluetooth.ChromeOS.FastPair.Pairing.Result.SubsequentPairingProtocol";
const char kFastPairPairResultRetroactiveMetric[] =
    "Bluetooth.ChromeOS.FastPair.Pairing.Result.RetroactivePairingProtocol";
const char kFastPairAccountKeyWriteResultInitialMetric[] =
    "Bluetooth.ChromeOS.FastPair.AccountKey.Write.Result."
    "InitialPairingProtocol";
const char kFastPairAccountKeyWriteResultSubsequentMetric[] =
    "Bluetooth.ChromeOS.FastPair.AccountKey.Write.Result."
    "SubsequentPairingProtocol";
const char kFastPairAccountKeyWriteResultRetroactiveMetric[] =
    "Bluetooth.ChromeOS.FastPair.AccountKey.Write.Result."
    "RetroactivePairingProtocol";
const char kFastPairAccountKeyWriteFailureInitialMetric[] =
    "Bluetooth.ChromeOS.FastPair.AccountKey.Failure.InitialPairingProtocol";
const char kFastPairAccountKeyWriteFailureRetroactiveMetric[] =
    "Bluetooth.ChromeOS.FastPair.AccountKey.Failure.RetroactivePairingProtocol";
const char kKeyGenerationResultMetric[] =
    "Bluetooth.ChromeOS.FastPair.KeyGenerationResult";
const char kDataEncryptorCreateResultMetric[] =
    "Bluetooth.ChromeOS.FastPair.FastPairDataEncryptor.CreateResult";
const char kWriteKeyBasedCharacteristicResult[] =
    "Bluetooth.ChromeOS.FastPair.KeyBasedPairing.Write.Result";
const char kWriteKeyBasedCharacteristicPairFailure[] =
    "Bluetooth.ChromeOS.FastPair.KeyBasedPairing.Write.PairFailure";
const char kWriteKeyBasedCharacteristicGattError[] =
    "Bluetooth.ChromeOS.FastPair.KeyBasedPairing.Write.GattErrorReason";
const char kNotifyKeyBasedCharacteristicTime[] =
    "Bluetooth.ChromeOS.FastPair.KeyBasedPairing.NotifyTime";
const char kKeyBasedCharacteristicDecryptTime[] =
    "Bluetooth.ChromeOS.FastPair.KeyBasedPairing.DecryptTime";
const char kKeyBasedCharacteristicDecryptResult[] =
    "Bluetooth.ChromeOS.FastPair.KeyBasedPairing.DecryptResult";
const char kWritePasskeyCharacteristicResult[] =
    "Bluetooth.ChromeOS.FastPair.Passkey.Write.Result";
const char kWritePasskeyCharacteristicPairFailure[] =
    "Bluetooth.ChromeOS.FastPair.Passkey.Write.PairFailure";
const char kWritePasskeyCharacteristicGattError[] =
    "Bluetooth.ChromeOS.FastPair.Passkey.Write.GattErrorReason";
const char kNotifyPasskeyCharacteristicTime[] =
    "Bluetooth.ChromeOS.FastPair.Passkey.NotifyTime";
const char kPasskeyCharacteristicDecryptTime[] =
    "Bluetooth.ChromeOS.FastPair.Passkey.Decrypt.Time";
const char kPasskeyCharacteristicDecryptResult[] =
    "Bluetooth.ChromeOS.FastPair.Passkey.Decrypt.Result";
const char kWriteAccountKeyCharacteristicResult[] =
    "Bluetooth.ChromeOS.FastPair.AccountKey.Write.Result";
const char kWriteAccountKeyCharacteristicGattError[] =
    "Bluetooth.ChromeOS.FastPair.AccountKey.Write.GattErrorReason";
const char kWriteAccountKeyTime[] =
    "Bluetooth.ChromeOS.FastPair.AccountKey.Write.TotalTime";
const char kTotalDataEncryptorCreateTime[] =
    "Bluetooth.ChromeOS.FastPair.FastPairDataEncryptor.CreateTime";
const char kMessageStreamReceiveResult[] =
    "Bluetooth.ChromeOS.FastPair.MessageStream.Receive.Result";
const char kMessageStreamReceiveError[] =
    "Bluetooth.ChromeOS.FastPair.MessageStream.Receive.ErrorReason";
const char kMessageStreamConnectToServiceError[] =
    "Bluetooth.ChromeOS.FastPair.MessageStream.ConnectToService.ErrorReason";
const char kMessageStreamConnectToServiceResult[] =
    "Bluetooth.ChromeOS.FastPair.MessageStream.ConnectToService.Result";
const char kMessageStreamConnectToServiceTime[] =
    "Bluetooth.ChromeOS.FastPair.MessageStream.ConnectToService."
    "TotalConnectTime";
const char kDeviceMetadataFetchResult[] =
    "Bluetooth.ChromeOS.FastPair.DeviceMetadataFetcher.Result";
const char kDeviceMetadataFetchNetError[] =
    "Bluetooth.ChromeOS.FastPair.DeviceMetadataFetcher.Get.NetError";
const char kDeviceMetadataFetchHttpResponseError[] =
    "Bluetooth.ChromeOS.FastPair.DeviceMetadataFetcher.Get.HttpResponseError";
const char kFootprintsFetcherDeleteResult[] =
    "Bluetooth.ChromeOS.FastPair.FootprintsFetcher.Delete.Result";
const char kFootprintsFetcherDeleteNetError[] =
    "Bluetooth.ChromeOS.FastPair.FootprintsFetcher.Delete.NetError";
const char kFootprintsFetcherDeleteHttpResponseError[] =
    "Bluetooth.ChromeOS.FastPair.FootprintsFetcher.Delete.HttpResponseError";
const char kFootprintsFetcherPostResult[] =
    "Bluetooth.ChromeOS.FastPair.FootprintsFetcher.Post.Result";
const char kFootprintsFetcherPostNetError[] =
    "Bluetooth.ChromeOS.FastPair.FootprintsFetcher.Post.NetError";
const char kFootprintsFetcherPostHttpResponseError[] =
    "Bluetooth.ChromeOS.FastPair.FootprintsFetcher.Post.HttpResponseError";
const char kFootprintsFetcherGetResult[] =
    "Bluetooth.ChromeOS.FastPair.FootprintsFetcher.Get.Result";
const char kFootprintsFetcherGetNetError[] =
    "Bluetooth.ChromeOS.FastPair.FootprintsFetcher.Get.NetError";
const char kFootprintsFetcherGetHttpResponseError[] =
    "Bluetooth.ChromeOS.FastPair.FootprintsFetcher.Get.HttpResponseError";
const char kFastPairRepositoryCacheResult[] =
    "Bluetooth.ChromeOS.FastPair.FastPairRepository.Cache.Result";
const char kHandshakeResult[] = "Bluetooth.ChromeOS.FastPair.Handshake.Result";
const char kFastPairHandshakeStepInitial[] =
    "Bluetooth.ChromeOS.FastPair.Handshake.Steps.InitialPairingProtocol";
const char kFastPairHandshakeStepSubsequent[] =
    "Bluetooth.ChromeOS.FastPair.Handshake.Steps.SubsequentPairingProtocol";
const char kFastPairHandshakeStepRetroactive[] =
    "Bluetooth.ChromeOS.FastPair.Handshake.Steps.RetroactivePairingProtocol";
const char kHandshakeFailureReason[] =
    "Bluetooth.ChromeOS.FastPair.Handshake.FailureReason";
const char kBleScanSessionResult[] =
    "Bluetooth.ChromeOS.FastPair.Scanner.StartSession.Result";
const char kBleScanFilterResult[] =
    "Bluetooth.ChromeOS.FastPair.CreateScanFilter.Result";
const char kFastPairVersion[] =
    "Bluetooth.ChromeOS.FastPair.Discovered.Version";
const char kNavigateToSettings[] =
    "Bluetooth.ChromeOS.FastPair.NavigateToSettings.Result";
const char kConnectDeviceResult[] =
    "Bluetooth.ChromeOS.FastPair.ConnectDevice.Result";
const char kPairDeviceResult[] =
    "Bluetooth.ChromeOS.FastPair.PairDevice.Result";
const char kPairDeviceErrorReason[] =
    "Bluetooth.ChromeOS.FastPair.PairDevice.ErrorReason";
const char kConfirmPasskeyAskTime[] =
    "Bluetooth.ChromeOS.FastPair.RequestPasskey.Latency";
const char kConfirmPasskeyConfirmTime[] =
    "Bluetooth.ChromeOS.FastPair.ConfirmPasskey.Latency";
const char kFastPairRetryCount[] =
    "Bluetooth.ChromeOS.FastPair.PairRetry.Count";
const char kSavedDeviceRemoveResult[] =
    "Bluetooth.ChromeOS.FastPair.SavedDevices.Remove.Result";
const char kSavedDeviceUpdateOptInStatusInitialResult[] =
    "Bluetooth.ChromeOS.FastPair.SavedDevices.UpdateOptInStatus.Result."
    "InitialPairingProtocol";
const char kSavedDeviceUpdateOptInStatusRetroactiveResult[] =
    "Bluetooth.ChromeOS.FastPair.SavedDevices.UpdateOptInStatus.Result."
    "RetroactivePairingProtocol";
const char kSavedDeviceUpdateOptInStatusSubsequentResult[] =
    "Bluetooth.ChromeOS.FastPair.SavedDevices.UpdateOptInStatus.Result."
    "SubsequentPairingProtocol";
const char kSavedDeviceGetDevicesResult[] =
    "Bluetooth.ChromeOS.FastPair.SavedDevices.GetSavedDevices.Result";
const char kSavedDevicesTotalUxLoadTime[] =
    "Bluetooth.ChromeOS.FastPair.SavedDevices.TotalUxLoadTime";
const char kSavedDevicesCount[] =
    "Bluetooth.ChromeOS.FastPair.SavedDevices.DeviceCount";
constexpr char kFastPairGattConnectionStep[] = "FastPair.GattConnection";
constexpr char kProtocolPairingStepInitial[] =
    "FastPair.InitialPairing.Pairing";
constexpr char kProtocolPairingStepSubsequent[] =
    "FastPair.SubsequentPairing.Pairing";
constexpr char kInitialSuccessFunnelMetric[] = "FastPair.InitialPairing";
constexpr char kSubsequentSuccessFunnelMetric[] = "FastPair.SubsequentPairing";
constexpr char kRetroactiveSuccessFunnelMetric[] =
    "FastPair.RetroactivePairing";
constexpr char kInitializePairingProcessInitial[] =
    "FastPair.InitialPairing.Initialization";
constexpr char kInitializePairingProcessSubsequent[] =
    "FastPair.SubsequentPairing.Initialization";
constexpr char kInitializePairingProcessRetroactive[] =
    "FastPair.RetroactivePairing.Initialization";
constexpr char kInitializePairingProcessFailureReasonInitial[] =
    "FastPair.InitialPairing.Initialization.FailureReason";
constexpr char kInitializePairingProcessFailureReasonSubsequent[] =
    "FastPair.SubsequentPairing.Initialization.FailureReason";
constexpr char kInitializePairingProcessFailureReasonRetroactive[] =
    "FastPair.RetroactivePairing.Initialization.FailureReason";
constexpr char kInitializePairingProcessRetriesBeforeSuccessInitial[] =
    "FastPair.InitialPairing.Initialization.RetriesBeforeSuccess";
constexpr char kInitializePairingProcessRetriesBeforeSuccessSubsequent[] =
    "FastPair.SubsequentPairing.Initialization.RetriesBeforeSuccess";
constexpr char kInitializePairingProcessRetriesBeforeSuccessRetroactive[] =
    "FastPair.RetroactivePairing.Initialization.RetriesBeforeSuccess";
const char kHandshakeEffectiveSuccessRate[] =
    "FastPair.Handshake.EffectiveSuccessRate";
const char kHandshakeAttemptCount[] = "FastPair.Handshake.AttemptCount";
const char kGattServiceDiscoveryTime[] =
    "FastPair.GattServiceDiscovery.Latency";
const char kCreateBondTime[] = "FastPair.CreateBond.Latency";
const char kPasskeyNotify[] = "FastPair.PasskeyNotify.Latency";
const char kKeyBasedNotify[] = "FastPair.KeyBasedNotify.Latency";
const char kPasskeyWriteRequest[] = "FastPair.PasskeyWriteRequest.Latency";
const char kKeyBasedWriteRequest[] = "FastPair.KeyBasedWriteRequest.Latency";

const std::string GetEngagementFlowInitialModelIdMetric(
    const ash::quick_pair::Device& device) {
  return std::string(kEngagementFlowInitialMetric) + "." +
         GetFastPairTrackedModelId(device.metadata_id());
}

const std::string GetEngagementFlowSubsequentModelIdMetric(
    const ash::quick_pair::Device& device) {
  return std::string(kEngagementFlowSubsequentMetric) + "." +
         GetFastPairTrackedModelId(device.metadata_id());
}

const std::string GetRetroactiveEngagementFlowModelIdMetric(
    const ash::quick_pair::Device& device) {
  return std::string(kRetroactiveEngagementFlowMetric) + "." +
         GetFastPairTrackedModelId(device.metadata_id());
}

// The retroactive engagement flow doesn't record retroactive successes
// properly due to b/240581398, so we use the account key write metric
// to record metrics split by model ID.
const std::string GetAccountKeyWriteResultRetroactiveModelIdMetric(
    const ash::quick_pair::Device& device) {
  return std::string(kFastPairAccountKeyWriteResultRetroactiveMetric) + "." +
         GetFastPairTrackedModelId(device.metadata_id());
}

std::optional<std::string>
GetEngagementFunnelInitialDeviceTypeNotificationTypeMetric(
    const nearby::fastpair::Device& device_metadata) {
  std::optional<std::string> device_type =
      GetFastPairDeviceType(device_metadata);
  std::optional<std::string> notification_type =
      GetFastPairNotificationType(device_metadata);

  if (!device_type || !notification_type) {
    return std::nullopt;
  }

  return std::string(kEngagementFlowInitialMetric) + "." + device_type.value() +
         "." + notification_type.value();
}

std::optional<std::string>
GetEngagementFunnelSubsequentDeviceTypeNotificationTypeMetric(
    const nearby::fastpair::Device& device_metadata) {
  std::optional<std::string> device_type =
      GetFastPairDeviceType(device_metadata);
  std::optional<std::string> notification_type =
      GetFastPairNotificationType(device_metadata);

  if (!device_type || !notification_type) {
    return std::nullopt;
  }

  return std::string(kEngagementFlowSubsequentMetric) + "." +
         device_type.value() + "." + notification_type.value();
}

std::optional<std::string>
GetEngagementFunnelRetroactiveDeviceTypeNotificationTypeMetric(
    const nearby::fastpair::Device& device_metadata) {
  std::optional<std::string> device_type =
      GetFastPairDeviceType(device_metadata);
  std::optional<std::string> notification_type =
      GetFastPairNotificationType(device_metadata);

  if (!device_type || !notification_type) {
    return std::nullopt;
  }

  return std::string(kRetroactiveEngagementFlowMetric) + "." +
         device_type.value() + "." + notification_type.value();
}

}  // namespace

namespace ash {
namespace quick_pair {

void RecordFastPairDeviceAndNotificationSpecificEngagementFlow(
    const Device& device,
    const nearby::fastpair::Device& device_details,
    FastPairEngagementFlowEvent event) {
  std::optional<std::string> funnel_name;

  switch (device.protocol()) {
    case Protocol::kFastPairInitial:
      funnel_name = GetEngagementFunnelInitialDeviceTypeNotificationTypeMetric(
          device_details);

      if (!funnel_name) {
        break;
      }

      base::UmaHistogramSparse(funnel_name.value(), static_cast<int>(event));
      break;
    // The retroactive pairing flow is not included here because it does not
    // involve a discovery notification or have an error notification on
    // failures. It's flow is captured in `FastPairRetroactiveFlowEvent`.
    case Protocol::kFastPairRetroactive:
      break;
    case Protocol::kFastPairSubsequent:
      funnel_name =
          GetEngagementFunnelSubsequentDeviceTypeNotificationTypeMetric(
              device_details);
      if (!funnel_name) {
        break;
      }

      base::UmaHistogramSparse(funnel_name.value(), static_cast<int>(event));
      break;
  }
}

void RecordFastPairDeviceAndNotificationSpecificRetroactiveEngagementFlow(
    const Device& device,
    const nearby::fastpair::Device& device_details,
    FastPairRetroactiveEngagementFlowEvent event) {
  std::optional<std::string> funnel_name;

  switch (device.protocol()) {
    case Protocol::kFastPairInitial:
      break;
    // This is only implemented for the retroactive pairing scenario since it's
    // flow is unique compared to the initial and subsequent flow : it shows
    // an associate account notification to start the scenario, whereas initial
    // and subsequent show a discovery notification, and there is no error
    // notification if there is a failure.
    case Protocol::kFastPairRetroactive:
      funnel_name =
          GetEngagementFunnelRetroactiveDeviceTypeNotificationTypeMetric(
              device_details);

      if (!funnel_name) {
        break;
      }

      base::UmaHistogramSparse(funnel_name.value(), static_cast<int>(event));
      break;
    case Protocol::kFastPairSubsequent:
      break;
  }
}

void AttemptRecordingFastPairEngagementFlow(const Device& device,
                                            FastPairEngagementFlowEvent event) {
  switch (device.protocol()) {
    case Protocol::kFastPairInitial:
      base::UmaHistogramSparse(kEngagementFlowInitialMetric,
                               static_cast<int>(event));
      // Also record engagement flow metrics split per tracked model ID.
      base::UmaHistogramSparse(GetEngagementFlowInitialModelIdMetric(device),
                               static_cast<int>(event));
      break;
    case Protocol::kFastPairRetroactive:
      break;
    case Protocol::kFastPairSubsequent:
      base::UmaHistogramSparse(kEngagementFlowSubsequentMetric,
                               static_cast<int>(event));
      // Also record engagement flow metrics split per tracked model ID.
      base::UmaHistogramSparse(GetEngagementFlowSubsequentModelIdMetric(device),
                               static_cast<int>(event));
      break;
  }
}

void RecordInitialSuccessFunnelFlow(FastPairInitialSuccessFunnelEvent event) {
  base::UmaHistogramEnumeration(kInitialSuccessFunnelMetric, event);
}

void RecordSubsequentSuccessFunnelFlow(
    FastPairSubsequentSuccessFunnelEvent event) {
  base::UmaHistogramEnumeration(kSubsequentSuccessFunnelMetric, event);
}

void RecordRetroactiveSuccessFunnelFlow(
    FastPairRetroactiveSuccessFunnelEvent event) {
  base::UmaHistogramEnumeration(kRetroactiveSuccessFunnelMetric, event);
}

void RecordFastPairInitializePairingProcessEvent(
    const Device& device,
    FastPairInitializePairingProcessEvent event) {
  switch (device.protocol()) {
    case Protocol::kFastPairInitial:
      base::UmaHistogramEnumeration(kInitializePairingProcessInitial, event);
      break;
    case Protocol::kFastPairRetroactive:
      base::UmaHistogramEnumeration(kInitializePairingProcessRetroactive,
                                    event);
      break;
    case Protocol::kFastPairSubsequent:
      base::UmaHistogramEnumeration(kInitializePairingProcessSubsequent, event);
      break;
  }
}

void RecordInitializationFailureReason(const Device& device,
                                       PairFailure failure_reason) {
  switch (device.protocol()) {
    case Protocol::kFastPairInitial:
      base::UmaHistogramEnumeration(
          kInitializePairingProcessFailureReasonInitial, failure_reason);
      break;
    case Protocol::kFastPairRetroactive:
      base::UmaHistogramEnumeration(
          kInitializePairingProcessFailureReasonRetroactive, failure_reason);
      break;
    case Protocol::kFastPairSubsequent:
      base::UmaHistogramEnumeration(
          kInitializePairingProcessFailureReasonSubsequent, failure_reason);
      break;
  }
}

void RecordInitializationRetriesBeforeSuccess(const Device& device,
                                              int num_retries_before_success) {
  switch (device.protocol()) {
    case Protocol::kFastPairInitial:
      base::UmaHistogramExactLinear(
          kInitializePairingProcessRetriesBeforeSuccessInitial,
          num_retries_before_success,
          /*exclusive_max=*/10);
      break;
    case Protocol::kFastPairRetroactive:
      base::UmaHistogramExactLinear(
          kInitializePairingProcessRetriesBeforeSuccessRetroactive,
          num_retries_before_success,
          /*exclusive_max=*/10);
      break;
    case Protocol::kFastPairSubsequent:
      base::UmaHistogramExactLinear(
          kInitializePairingProcessRetriesBeforeSuccessSubsequent,
          num_retries_before_success,
          /*exclusive_max=*/10);
      break;
  }
}

void AttemptRecordingTotalUxPairTime(const Device& device,
                                     base::TimeDelta total_pair_time) {
  switch (device.protocol()) {
    case Protocol::kFastPairInitial:
      base::UmaHistogramCustomTimes(kTotalUxPairTimeInitialMetric,
                                    total_pair_time, base::Milliseconds(1),
                                    base::Seconds(25), 50);
      break;
    case Protocol::kFastPairRetroactive:
      break;
    case Protocol::kFastPairSubsequent:
      base::UmaHistogramCustomTimes(kTotalUxPairTimeSubsequentMetric,
                                    total_pair_time, base::Milliseconds(1),
                                    base::Seconds(25), 50);
      break;
  }
}

void AttemptRecordingFastPairRetroactiveEngagementFlow(
    const Device& device,
    FastPairRetroactiveEngagementFlowEvent event) {
  switch (device.protocol()) {
    case Protocol::kFastPairInitial:
    case Protocol::kFastPairSubsequent:
      break;
    case Protocol::kFastPairRetroactive:
      base::UmaHistogramSparse(kRetroactiveEngagementFlowMetric,
                               static_cast<int>(event));
      // Also record engagement flow metrics split per tracked model ID.
      base::UmaHistogramSparse(
          GetRetroactiveEngagementFlowModelIdMetric(device),
          static_cast<int>(event));
      break;
  }
}

void RecordPairingMethod(PairingMethod method) {
  base::UmaHistogramEnumeration(kPairingMethodMetric, method);
}

void RecordRetroactivePairingResult(bool success) {
  base::UmaHistogramBoolean(kRetroactivePairingResultMetric, success);
}

void RecordTotalGattConnectionTime(base::TimeDelta total_gatt_connection_time) {
  base::UmaHistogramTimes(kTotalGattConnectionTimeMetric,
                          total_gatt_connection_time);
}

void RecordGattConnectionResult(bool success) {
  base::UmaHistogramBoolean(kGattConnectionResult, success);
}

void RecordGattConnectionErrorCode(
    device::BluetoothDevice::ConnectErrorCode error_code) {
  base::UmaHistogramEnumeration(
      kGattConnectionErrorMetric, error_code,
      device::BluetoothDevice::ConnectErrorCode::NUM_CONNECT_ERROR_CODES);
}

void RecordEffectiveGattConnectionSuccess(bool success) {
  base::UmaHistogramBoolean(kGattConnectionEffectiveSuccessRate, success);
}

void RecordGattConnectionAttemptCount(int num_attempts) {
  base::UmaHistogramExactLinear(kGattConnectionAttemptCount, num_attempts,
                                /*exclusive_max=*/10);
}

void RecordGattRetryFailureReason(PairFailure failure) {
  base::UmaHistogramEnumeration(kFastPairGattRetryFailureReason, failure);
}

void RecordPairingResult(const Device& device, bool success) {
  switch (device.protocol()) {
    case Protocol::kFastPairInitial:
      base::UmaHistogramBoolean(kFastPairPairResultInitialMetric, success);
      break;
    case Protocol::kFastPairRetroactive:
      base::UmaHistogramBoolean(kFastPairPairResultRetroactiveMetric, success);
      break;
    case Protocol::kFastPairSubsequent:
      base::UmaHistogramBoolean(kFastPairPairResultSubsequentMetric, success);
      break;
  }
}

void RecordPairingFailureReason(const Device& device, PairFailure failure) {
  switch (device.protocol()) {
    case Protocol::kFastPairInitial:
      base::UmaHistogramEnumeration(kFastPairPairFailureInitialMetric, failure);
      break;
    case Protocol::kFastPairRetroactive:
      base::UmaHistogramEnumeration(kFastPairPairFailureRetroactiveMetric,
                                    failure);
      break;
    case Protocol::kFastPairSubsequent:
      base::UmaHistogramEnumeration(kFastPairPairFailureSubsequentMetric,
                                    failure);
      break;
  }
}

void RecordAccountKeyFailureReason(const Device& device,
                                   AccountKeyFailure failure) {
  switch (device.protocol()) {
    case Protocol::kFastPairInitial:
      base::UmaHistogramEnumeration(
          kFastPairAccountKeyWriteFailureInitialMetric, failure);
      break;
    case Protocol::kFastPairRetroactive:
      base::UmaHistogramEnumeration(
          kFastPairAccountKeyWriteFailureRetroactiveMetric, failure);
      break;
    case Protocol::kFastPairSubsequent:
      break;
  }
}

void RecordAccountKeyResult(const Device& device, bool success) {
  switch (device.protocol()) {
    case Protocol::kFastPairInitial:
      base::UmaHistogramBoolean(kFastPairAccountKeyWriteResultInitialMetric,
                                success);
      break;
    case Protocol::kFastPairRetroactive:
      base::UmaHistogramBoolean(kFastPairAccountKeyWriteResultRetroactiveMetric,
                                success);
      // Also record engagement flow metrics split per tracked model ID.
      base::UmaHistogramBoolean(
          GetAccountKeyWriteResultRetroactiveModelIdMetric(device), success);
      break;
    case Protocol::kFastPairSubsequent:
      base::UmaHistogramBoolean(kFastPairAccountKeyWriteResultSubsequentMetric,
                                success);
      break;
  }
}

void RecordKeyPairGenerationResult(bool success) {
  base::UmaHistogramBoolean(kKeyGenerationResultMetric, success);
}

void RecordDataEncryptorCreateResult(bool success) {
  base::UmaHistogramBoolean(kDataEncryptorCreateResultMetric, success);
}

void RecordWriteKeyBasedCharacteristicResult(bool success) {
  base::UmaHistogramBoolean(kWriteKeyBasedCharacteristicResult, success);
}

void RecordWriteKeyBasedCharacteristicPairFailure(PairFailure failure) {
  base::UmaHistogramEnumeration(kWriteKeyBasedCharacteristicPairFailure,
                                failure);
}

void RecordWriteRequestGattError(
    device::BluetoothGattService::GattErrorCode error) {
  base::UmaHistogramEnumeration(kWriteKeyBasedCharacteristicGattError, error);
}

void RecordNotifyKeyBasedCharacteristicTime(base::TimeDelta total_notify_time) {
  base::UmaHistogramTimes(kNotifyKeyBasedCharacteristicTime, total_notify_time);
}

void RecordKeyBasedCharacteristicDecryptTime(base::TimeDelta decrypt_time) {
  base::UmaHistogramTimes(kKeyBasedCharacteristicDecryptTime, decrypt_time);
}

void RecordKeyBasedCharacteristicDecryptResult(bool success) {
  base::UmaHistogramBoolean(kKeyBasedCharacteristicDecryptResult, success);
}

void RecordWritePasskeyCharacteristicResult(bool success) {
  base::UmaHistogramBoolean(kWritePasskeyCharacteristicResult, success);
}

void RecordWritePasskeyCharacteristicPairFailure(PairFailure failure) {
  base::UmaHistogramEnumeration(kWritePasskeyCharacteristicPairFailure,
                                failure);
}

void RecordWritePasskeyGattError(
    device::BluetoothGattService::GattErrorCode error) {
  base::UmaHistogramEnumeration(kWritePasskeyCharacteristicGattError, error);
}

void RecordNotifyPasskeyCharacteristicTime(base::TimeDelta total_notify_time) {
  base::UmaHistogramTimes(kNotifyPasskeyCharacteristicTime, total_notify_time);
}

void RecordPasskeyCharacteristicDecryptTime(base::TimeDelta decrypt_time) {
  base::UmaHistogramTimes(kPasskeyCharacteristicDecryptTime, decrypt_time);
}

void RecordPasskeyCharacteristicDecryptResult(bool success) {
  base::UmaHistogramBoolean(kPasskeyCharacteristicDecryptResult, success);
}

void RecordWriteAccountKeyCharacteristicResult(bool success) {
  base::UmaHistogramBoolean(kWriteAccountKeyCharacteristicResult, success);
}

void RecordWriteAccountKeyGattError(
    device::BluetoothGattService::GattErrorCode error) {
  base::UmaHistogramEnumeration(kWriteAccountKeyCharacteristicGattError, error);
}

void RecordWriteAccountKeyTime(base::TimeDelta write_time) {
  base::UmaHistogramTimes(kWriteAccountKeyTime, write_time);
}

void RecordTotalDataEncryptorCreateTime(base::TimeDelta total_create_time) {
  base::UmaHistogramTimes(kTotalDataEncryptorCreateTime, total_create_time);
}

void RecordMessageStreamReceiveResult(bool success) {
  base::UmaHistogramBoolean(kMessageStreamReceiveResult, success);
}

void RecordMessageStreamReceiveError(
    device::BluetoothSocket::ErrorReason error) {
  base::UmaHistogramEnumeration(kMessageStreamReceiveError, error);
}

void RecordMessageStreamConnectToServiceResult(bool success) {
  base::UmaHistogramBoolean(kMessageStreamConnectToServiceResult, success);
}

void RecordMessageStreamConnectToServiceError(const std::string& error) {
  base::UmaHistogramEnumeration(kMessageStreamConnectToServiceError,
                                GetConnectToServiceError(error));
}

void RecordMessageStreamConnectToServiceTime(
    base::TimeDelta total_connect_time) {
  base::UmaHistogramTimes(kMessageStreamConnectToServiceTime,
                          total_connect_time);
}

void RecordDeviceMetadataFetchResult(const FastPairHttpResult& result) {
  base::UmaHistogramBoolean(kDeviceMetadataFetchResult, result.IsSuccess());

  if (result.net_error()) {
    base::UmaHistogramSparse(kDeviceMetadataFetchNetError,
                             -*result.net_error());
  }

  if (result.http_response_error()) {
    base::UmaHistogramSparse(kDeviceMetadataFetchHttpResponseError,
                             *result.http_response_error());
  }
}

void RecordFootprintsFetcherDeleteResult(const FastPairHttpResult& result) {
  base::UmaHistogramBoolean(kFootprintsFetcherDeleteResult, result.IsSuccess());

  if (result.net_error()) {
    base::UmaHistogramSparse(kFootprintsFetcherDeleteNetError,
                             -*result.net_error());
  }

  if (result.http_response_error()) {
    base::UmaHistogramSparse(kFootprintsFetcherDeleteHttpResponseError,
                             *result.http_response_error());
  }
}

void RecordFootprintsFetcherPostResult(const FastPairHttpResult& result) {
  base::UmaHistogramBoolean(kFootprintsFetcherPostResult, result.IsSuccess());

  if (result.net_error()) {
    base::UmaHistogramSparse(kFootprintsFetcherPostNetError,
                             -*result.net_error());
  }

  if (result.http_response_error()) {
    base::UmaHistogramSparse(kFootprintsFetcherPostHttpResponseError,
                             *result.http_response_error());
  }
}

void RecordFootprintsFetcherGetResult(const FastPairHttpResult& result) {
  base::UmaHistogramBoolean(kFootprintsFetcherGetResult, result.IsSuccess());

  if (result.net_error()) {
    base::UmaHistogramSparse(kFootprintsFetcherGetNetError,
                             -*result.net_error());
  }

  if (result.http_response_error()) {
    base::UmaHistogramSparse(kFootprintsFetcherGetHttpResponseError,
                             *result.http_response_error());
  }
}

void RecordFastPairRepositoryCacheResult(bool success) {
  base::UmaHistogramBoolean(kFastPairRepositoryCacheResult, success);
}

void RecordGattInitializationStep(
    FastPairGattConnectionSteps initialization_step) {
  base::UmaHistogramEnumeration(kFastPairGattConnectionStep,
                                initialization_step);
}

void RecordEffectiveHandshakeSuccess(bool success) {
  base::UmaHistogramBoolean(kHandshakeEffectiveSuccessRate, success);
}

void RecordHandshakeAttemptCount(int num_attempts) {
  base::UmaHistogramExactLinear(kHandshakeAttemptCount, num_attempts,
                                /*exclusive_max=*/10);
}

void RecordHandshakeResult(bool success) {
  base::UmaHistogramBoolean(kHandshakeResult, success);
}

void RecordHandshakeFailureReason(HandshakeFailureReason failure_reason) {
  base::UmaHistogramEnumeration(kHandshakeFailureReason, failure_reason);
}

void RecordProtocolPairingStep(FastPairProtocolPairingSteps pairing_step,
                               const Device& device) {
  switch (device.protocol()) {
    case Protocol::kFastPairInitial:
      base::UmaHistogramEnumeration(kProtocolPairingStepInitial, pairing_step);
      break;
    case Protocol::kFastPairSubsequent:
      base::UmaHistogramEnumeration(kProtocolPairingStepSubsequent,
                                    pairing_step);
      break;
    case Protocol::kFastPairRetroactive:
      break;
  }
}

void RecordHandshakeStep(FastPairHandshakeSteps handshake_step,
                         const Device& device) {
  switch (device.protocol()) {
    case Protocol::kFastPairInitial:
      base::UmaHistogramEnumeration(kFastPairHandshakeStepInitial,
                                    handshake_step);
      break;
    case Protocol::kFastPairRetroactive:
      base::UmaHistogramEnumeration(kFastPairHandshakeStepRetroactive,
                                    handshake_step);
      break;
    case Protocol::kFastPairSubsequent:
      base::UmaHistogramEnumeration(kFastPairHandshakeStepSubsequent,
                                    handshake_step);
      break;
  }
}

void RecordBluetoothLowEnergyScannerStartSessionResult(bool success) {
  base::UmaHistogramBoolean(kBleScanSessionResult, success);
}

void RecordBluetoothLowEnergyScanFilterResult(bool success) {
  base::UmaHistogramBoolean(kBleScanFilterResult, success);
}

void RecordFastPairDiscoveredVersion(FastPairVersion version) {
  base::UmaHistogramEnumeration(kFastPairVersion, version);
}

void RecordNavigateToSettingsResult(bool success) {
  base::UmaHistogramBoolean(kNavigateToSettings, success);
}

void RecordConnectDeviceResult(bool success) {
  base::UmaHistogramBoolean(kConnectDeviceResult, success);
}

void RecordPairDeviceResult(bool success) {
  base::UmaHistogramBoolean(kPairDeviceResult, success);
}

void RecordPairDeviceErrorReason(
    device::BluetoothDevice::ConnectErrorCode error_code) {
  base::UmaHistogramEnumeration(
      kPairDeviceErrorReason, error_code,
      device::BluetoothDevice::NUM_CONNECT_ERROR_CODES);
}

void RecordConfirmPasskeyConfirmTime(base::TimeDelta total_confirm_time) {
  base::UmaHistogramTimes(kConfirmPasskeyConfirmTime, total_confirm_time);
}

void RecordConfirmPasskeyAskTime(base::TimeDelta total_ask_time) {
  base::UmaHistogramTimes(kConfirmPasskeyAskTime, total_ask_time);
}

void RecordGattServiceDiscoveryTime(
    base::TimeDelta total_gatt_service_discovery_time) {
  base::UmaHistogramTimes(kGattServiceDiscoveryTime,
                          total_gatt_service_discovery_time);
}

void RecordCreateBondTime(base::TimeDelta total_create_bond_time) {
  base::UmaHistogramTimes(kCreateBondTime, total_create_bond_time);
}

void RecordPasskeyNotifyTime(base::TimeDelta total_passkey_notify_time) {
  base::UmaHistogramTimes(kPasskeyNotify, total_passkey_notify_time);
}

void RecordKeyBasedNotifyTime(base::TimeDelta total_key_based_notify_time) {
  base::UmaHistogramTimes(kKeyBasedNotify, total_key_based_notify_time);
}

void RecordPasskeyWriteRequestTime(
    base::TimeDelta total_passkey_write_request_time) {
  base::UmaHistogramTimes(kPasskeyWriteRequest,
                          total_passkey_write_request_time);
}

void RecordKeyBasedWriteRequestTime(
    base::TimeDelta total_key_based_write_request_time) {
  base::UmaHistogramTimes(kKeyBasedWriteRequest,
                          total_key_based_write_request_time);
}

void RecordPairFailureRetry(int num_retries) {
  base::UmaHistogramExactLinear(kFastPairRetryCount, num_retries,
                                /*exclusive_max=*/10);
}

void RecordSavedDevicesRemoveResult(bool success) {
  base::UmaHistogramBoolean(kSavedDeviceRemoveResult, success);
}

void RecordSavedDevicesUpdatedOptInStatusResult(const Device& device,
                                                bool success) {
  switch (device.protocol()) {
    case Protocol::kFastPairInitial:
      base::UmaHistogramBoolean(kSavedDeviceUpdateOptInStatusInitialResult,
                                success);
      break;
    case Protocol::kFastPairRetroactive:
      base::UmaHistogramBoolean(kSavedDeviceUpdateOptInStatusRetroactiveResult,
                                success);
      break;
    case Protocol::kFastPairSubsequent:
      base::UmaHistogramBoolean(kSavedDeviceUpdateOptInStatusSubsequentResult,
                                success);
      break;
  }
}

void RecordGetSavedDevicesResult(bool success) {
  base::UmaHistogramBoolean(kSavedDeviceGetDevicesResult, success);
}

void RecordSavedDevicesTotalUxLoadTime(base::TimeDelta total_load_time) {
  base::UmaHistogramCustomTimes(kSavedDevicesTotalUxLoadTime, total_load_time,
                                base::Milliseconds(1), base::Seconds(25), 50);
}

void RecordSavedDevicesCount(int num_devices) {
  base::UmaHistogramCounts100(kSavedDevicesCount, num_devices);
}

int ConvertFastPairVersionToInt(std::optional<DeviceFastPairVersion> version) {
  if (!version) {
    return 0;
  }

  switch (version.value()) {
    case DeviceFastPairVersion::kV1:
      return 1;
    case DeviceFastPairVersion::kHigherThanV1:
      return 2;
  }
}

int GetRSSI(const device::BluetoothDevice* bt_device) {
  int rssi = kUnknownRSSI;
  if (bt_device) {
    if (bt_device->GetInquiryRSSI().has_value()) {
      rssi = bt_device->GetInquiryRSSI().value();
    }
  }
  return rssi;
}

int GetTxPower(const device::BluetoothDevice* bt_device) {
  int tx_power = kUnknownTxPower;
  if (bt_device) {
    if (bt_device->GetInquiryTxPower().has_value()) {
      tx_power = bt_device->GetInquiryTxPower().value();
    }
  }
  return tx_power;
}

// TODO(b/266739400): There is currently no way to properly unittest these
// changes. The metrics team plans on implementing a way to mock out the
// structured metrics client in the near future. We should follow up and
// implement proper tests for these functions once that is available.
void RecordStructuredDiscoveryNotificationShown(
    const Device& device,
    const device::BluetoothDevice* bt_device) {
  CD_LOG(INFO, Feature::FP) << __func__;
  int model_id;
  if (!base::HexStringToInt(device.metadata_id(), &model_id)) {
    return;
  }
  int version = ConvertFastPairVersionToInt(device.version());
  int rssi = GetRSSI(bt_device);
  int tx_power = GetTxPower(bt_device);
  CD_LOG(VERBOSE, Feature::FP)
      << __func__ << ": RSSI: " << rssi << ", TxPower: " << tx_power;
  metrics::structured::StructuredMetricsClient::Record(std::move(
      metrics::structured::events::v2::fast_pair::DiscoveryNotificationShown()
          .SetProtocol(static_cast<int>(device.protocol()))
          .SetModelId(model_id)
          .SetFastPairVersion(version)
          .SetRSSI(rssi)
          .SetTxPower(tx_power)));
}

void RecordStructuredPairingStarted(const Device& device,
                                    const device::BluetoothDevice* bt_device) {
  CD_LOG(INFO, Feature::FP) << __func__;
  int model_id;
  if (!base::HexStringToInt(device.metadata_id(), &model_id)) {
    return;
  }
  int version = ConvertFastPairVersionToInt(device.version());
  int rssi = GetRSSI(bt_device);
  int tx_power = GetTxPower(bt_device);
  CD_LOG(VERBOSE, Feature::FP)
      << __func__ << ": RSSI: " << rssi << ", TxPower: " << tx_power;
  metrics::structured::StructuredMetricsClient::Record(
      std::move(metrics::structured::events::v2::fast_pair::PairingStart()
                    .SetProtocol(static_cast<int>(device.protocol()))
                    .SetModelId(model_id)
                    .SetFastPairVersion(version)
                    .SetRSSI(rssi)
                    .SetTxPower(tx_power)));
}

void RecordStructuredPairingComplete(const Device& device,
                                     const device::BluetoothDevice* bt_device) {
  CD_LOG(INFO, Feature::FP) << __func__;
  int model_id;
  if (!base::HexStringToInt(device.metadata_id(), &model_id)) {
    return;
  }
  int version = ConvertFastPairVersionToInt(device.version());
  int rssi = GetRSSI(bt_device);
  int tx_power = GetTxPower(bt_device);
  CD_LOG(VERBOSE, Feature::FP)
      << __func__ << ": RSSI: " << rssi << ", TxPower: " << tx_power;
  metrics::structured::StructuredMetricsClient::Record(
      std::move(metrics::structured::events::v2::fast_pair::PairingComplete()
                    .SetProtocol(static_cast<int>(device.protocol()))
                    .SetModelId(model_id)
                    .SetFastPairVersion(version)
                    .SetRSSI(rssi)
                    .SetTxPower(tx_power)));
}

void RecordStructuredPairFailure(const Device& device, PairFailure failure) {
  CD_LOG(INFO, Feature::FP) << __func__;
  int model_id;
  if (!base::HexStringToInt(device.metadata_id(), &model_id)) {
    return;
  }
  int version = ConvertFastPairVersionToInt(device.version());
  metrics::structured::StructuredMetricsClient::Record(
      std::move(metrics::structured::events::v2::fast_pair::PairFailure()
                    .SetProtocol(static_cast<int>(device.protocol()))
                    .SetModelId(model_id)
                    .SetReason(static_cast<int>(failure))
                    .SetFastPairVersion(version)));
}

}  // namespace quick_pair
}  // namespace ash
