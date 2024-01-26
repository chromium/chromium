// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import {NetworkStateProperties as Network} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';

import {CalibrationComponentStatus, CalibrationStatus, Component, ComponentRepairStatus, ComponentType, RmadErrorCode, State, StateResult} from './shimless_rma.mojom-webui.js';


export const fakeStates: StateResult[] = [
  {
    state: State.kWelcomeScreen,
    canExit: true,
    canGoBack: false,
    error: RmadErrorCode.kOk,
  },
  {
    state: State.kConfigureNetwork,
    canExit: true,
    canGoBack: true,
    error: RmadErrorCode.kOk,
  },
  {
    state: State.kUpdateOs,
    canExit: true,
    canGoBack: true,
    error: RmadErrorCode.kOk,
  },
  {
    state: State.kSelectComponents,
    canExit: true,
    canGoBack: true,
    error: RmadErrorCode.kOk,
  },
  {
    state: State.kChooseDestination,
    canExit: true,
    canGoBack: true,
    error: RmadErrorCode.kOk,
  },
  {
    state: State.kChooseWipeDevice,
    canExit: true,
    canGoBack: true,
    error: RmadErrorCode.kOk,
  },
  {
    state: State.kChooseWriteProtectDisableMethod,
    canExit: true,
    canGoBack: true,
    error: RmadErrorCode.kOk,
  },
  {
    state: State.kEnterRSUWPDisableCode,
    canExit: true,
    canGoBack: true,
    error: RmadErrorCode.kOk,
  },
  {
    state: State.kWaitForManualWPDisable,
    canExit: true,
    canGoBack: true,
    error: RmadErrorCode.kOk,
  },
  {
    state: State.kWPDisableComplete,
    canExit: true,
    canGoBack: true,
    error: RmadErrorCode.kOk,
  },
  {
    state: State.kUpdateRoFirmware,
    canExit: true,
    canGoBack: true,
    error: RmadErrorCode.kOk,
  },
  {
    state: State.kUpdateDeviceInformation,
    canExit: true,
    canGoBack: true,
    error: RmadErrorCode.kOk,
  },
  {
    state: State.kRestock,
    canExit: true,
    canGoBack: true,
    error: RmadErrorCode.kOk,
  },
  {
    state: State.kCheckCalibration,
    canExit: true,
    canGoBack: true,
    error: RmadErrorCode.kOk,
  },
  {
    state: State.kSetupCalibration,
    canExit: true,
    canGoBack: true,
    error: RmadErrorCode.kOk,
  },
  {
    state: State.kRunCalibration,
    canExit: true,
    canGoBack: true,
    error: RmadErrorCode.kOk,
  },
  {
    state: State.kProvisionDevice,
    canExit: true,
    canGoBack: true,
    error: RmadErrorCode.kOk,
  },
  {
    state: State.kWaitForManualWPEnable,
    canExit: true,
    canGoBack: true,
    error: RmadErrorCode.kOk,
  },
  {
    state: State.kFinalize,
    canExit: true,
    canGoBack: true,
    error: RmadErrorCode.kOk,
  },
  {
    state: State.kRepairComplete,
    canExit: true,
    canGoBack: true,
    error: RmadErrorCode.kOk,
  },
  {
    state: State.kUnknown,
    canExit: false,
    canGoBack: false,
    error: RmadErrorCode.kOk,
  },
  {
    state: State.kHardwareError,
    canExit: false,
    canGoBack: false,
    error: RmadErrorCode.kOk,
  },
  {
    state: State.kReboot,
    canExit: false,
    canGoBack: false,
    error: RmadErrorCode.kOk,
  },
];

export const fakeChromeVersion: string[] = [
  '89.0.1232.1',
  '92.0.999.0',
  '95.0.4444.123',
];

export const fakeRsuChallengeCode =
    'HRBXHV84NSTHT25WJECYQKB8SARWFTMSWNGFT2FVEEPX69VE99USV3QFBEANDVXGQVL93QK2M6P3DNV4';

export const fakeRsuChallengeQrCode: number[] =
    [0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0];

export const fakeComponents: Component[] = [
  {
    component: ComponentType.kCamera,
    state: ComponentRepairStatus.kOriginal,
    identifier: 'Camera_XYZ_1',
  },
  {
    component: ComponentType.kBattery,
    state: ComponentRepairStatus.kMissing,
    identifier: 'Battery_XYZ_Lithium',
  },
  {
    component: ComponentType.kTouchpad,
    state: ComponentRepairStatus.kOriginal,
    identifier: 'Touchpad_XYZ_2',
  },
];

// onboarding_select_components_page_test needs a components list covering all
// possible repair states.
export const fakeComponentsForRepairStateTest: Component[] = [
  {
    component: ComponentType.kAudioCodec,
    state: ComponentRepairStatus.kMissing,
    identifier: 'Audio_XYZ',
  },
  {
    component: ComponentType.kCamera,
    state: ComponentRepairStatus.kOriginal,
    identifier: 'Camera_XYZ_1',
  },
  {
    component: ComponentType.kBattery,
    state: ComponentRepairStatus.kMissing,
    identifier: 'Battery_XYZ_Lithium',
  },
  {
    component: ComponentType.kTouchpad,
    state: ComponentRepairStatus.kReplaced,
    identifier: 'Touchpad_XYZ_2',
  },
  {
    component: ComponentType.kStorage,
    state: ComponentRepairStatus.kMissing,
    identifier: 'Storage_XYZ',
  },
  {
    component: ComponentType.kVpdCached,
    state: ComponentRepairStatus.kMissing,
    identifier: 'VpdCached_XYZ',
  },
  {
    component: ComponentType.kNetwork,
    state: ComponentRepairStatus.kOriginal,
    identifier: 'Network_XYZ',
  },
  {
    component: ComponentType.kCamera,
    state: ComponentRepairStatus.kOriginal,
    identifier: 'Camera_XYZ_2',
  },
  {
    component: ComponentType.kTouchsreen,
    state: ComponentRepairStatus.kMissing,
    identifier: 'Touchscreen_XYZ',
  },
];

export const fakeCalibrationComponentsWithFails: CalibrationComponentStatus[] =
    [
      {
        component: ComponentType.kCamera,
        status: CalibrationStatus.kCalibrationFailed,
        progress: 0.0,
      },
      {
        component: ComponentType.kBattery,
        status: CalibrationStatus.kCalibrationComplete,
        progress: 1.0,
      },
      {
        component: ComponentType.kLidAccelerometer,
        status: CalibrationStatus.kCalibrationComplete,
        progress: 1.0,
      },
      {
        component: ComponentType.kBaseAccelerometer,
        status: CalibrationStatus.kCalibrationComplete,
        progress: 1.0,
      },
      {
        component: ComponentType.kTouchpad,
        status: CalibrationStatus.kCalibrationComplete,
        progress: 0.0,
      },
      {
        component: ComponentType.kScreen,
        status: CalibrationStatus.kCalibrationFailed,
        progress: 1.0,
      },
      {
        component: ComponentType.kBaseGyroscope,
        status: CalibrationStatus.kCalibrationFailed,
        progress: 1.0,
      },
    ];

export const fakeCalibrationComponentsWithoutFails:
    CalibrationComponentStatus[] = [
      {
        component: ComponentType.kCamera,
        status: CalibrationStatus.kCalibrationComplete,
        progress: 0.0,
      },
      {
        component: ComponentType.kBattery,
        status: CalibrationStatus.kCalibrationComplete,
        progress: 1.0,
      },
      {
        component: ComponentType.kBaseAccelerometer,
        status: CalibrationStatus.kCalibrationComplete,
        progress: 1.0,
      },
      {
        component: ComponentType.kLidAccelerometer,
        status: CalibrationStatus.kCalibrationComplete,
        progress: 1.0,
      },
      {
        component: ComponentType.kTouchpad,
        status: CalibrationStatus.kCalibrationComplete,
        progress: 0.0,
      },
    ];

export const fakeNetworks: Network[] = [
  OncMojo.getDefaultNetworkState(NetworkType.kWiFi, 'wifi0'),
];

export const fakeDeviceRegions: string[] = ['EMEA', 'APAC', 'AMER'];

export const fakeDeviceSkus: bigint[] = [BigInt(1), BigInt(2), BigInt(3)];

export const fakeDeviceCustomLabels: string[] =
    ['Custom-label 1', 'Custom-label 2', 'Custom-label 3', ''];

export const fakeDeviceSkuDescriptions: string[] = ['SKU 1', 'SKU 2', 'SKU 3'];

export const fakeLog =
    'Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod ' +
    'tempor incididunt ut labore et dolore magna aliqua. Vitae auctor eu ' +
    'augue ut lectus. Pellentesque habitant morbi tristique senectus et netus' +
    ' et. Felis eget nunc lobortis mattis aliquam faucibus purus. Aliquam ' +
    'etiam erat velit scelerisque. Tellus rutrum tellus pellentesque eu.\n' +
    'Curabitur gravida arcu ac tortor dignissim convallis aenean. Sagittis ' +
    'vitae et leo duis ut diam quam. Tristique sollicitudin nibh sit amet.\n' +
    'Cursus risus at ultrices mi tempus. Viverra accumsan in nisl nisi.\n' +
    'Nulla porttitor massa id neque aliquam. Vulputate sapien nec sagittis ' +
    'aliquam. Vel risus commodo viverra maecenas accumsan lacus vel ' +
    'facilisis. Urna cursus eget nunc scelerisque viverra mauris in aliquam.' +
    '\n' +
    'Sed ullamcorper morbi tincidunt ornare massa eget egestas purus ' +
    'viverra. Justo eget magna fermentum iaculis eu non diam.\n\n' +
    'Accumsan tortor posuere ac ut consequat semper viverra nam libero.\n' +
    'Potenti nullam ac tortor vitae purus faucibus ornare suspendisse sed.\n' +
    'Pharetra massa massa ultricies mi quis hendrerit dolor magna eget.\n' +
    'Velit egestas dui id ornare arcu odio ut. Quam pellentesque nec nam ' +
    'aliquam sem et tortor consequat id. Ut aliquam purus sit amet luctus ' +
    'venenatis lectus magna fringilla. Turpis massa sed elementum tempus ' +
    'egestas. Sed sed risus pretium quam vulputate dignissim suspendisse in ' +
    'est. Odio facilisis mauris sit amet massa vitae tortor. Purus sit amet ' +
    'volutpat consequat mauris nunc congue nisi. Commodo quis imperdiet massa' +
    ' tincidunt nunc pulvinar. Porttitor massa id neque aliquam vestibulum ' +
    'morbi. Ut consequat semper viverra nam libero justo.\n\n' +
    'Ornare arcu odio ut sem nulla pharetra diam sit. Nullam eget felis eget ' +
    'nunc lobortis mattis aliquam faucibus. Volutpat commodo sed egestas ' +
    'egestas fringilla. Arcu felis bibendum ut tristique. Condimentum vitae ' +
    'sapien pellentesque habitant morbi tristique senectus. Nisl suscipit ' +
    'adipiscing bibendum est ultricies integer quis auctor elit. Nunc ' +
    'aliquet bibendum enim facilisis. Cras pulvinar mattis nunc sed blandit ' +
    'libero volutpat sed. Aliquam ut porttitor leo a. Ultricies lacus sed ' +
    'turpis tincidunt id aliquet risus feugiat in. Magna ac placerat ' +
    'vestibulum lectus mauris ultrices. Et malesuada fames ac turpis egestas ' +
    'sed. Volutpat sed cras ornare arcu. Egestas egestas fringilla phasellus ' +
    'faucibus. Euismod nisi porta lorem mollis aliquam ut. Ut placerat orci ' +
    'nulla pellentesque dignissim enim.\n\n' +
    'Integer feugiat scelerisque varius morbi enim nunc. Aenean vel elit ' +
    'scelerisque mauris pellentesque pulvinar pellentesque habitant morbi.\n' +
    'In iaculis nunc sed augue lacus viverra vitae congue. Mus mauris vitae ' +
    'ultricies leo. Ullamcorper eget nulla facilisi etiam dignissim diam ' +
    'quis. Neque viverra justo nec ultrices dui sapien eget mi proin.\n' +
    'Facilisis leo vel fringilla est ullamcorper eget. Condimentum lacinia ' +
    'quis vel eros. Velit sed ullamcorper morbi tincidunt ornare massa. Urna ' +
    'porttitor rhoncus dolor purus non enim praesent elementum facilisis.\n' +
    'Tristique et egestas quis ipsum suspendisse.\n\n' +
    'Posuere lorem ipsum dolor sit amet consectetur adipiscing. Massa sapien ' +
    'faucibus et molestie ac feugiat sed lectus. Nunc id cursus metus ' +
    'aliquam eleifend mi in. Integer enim neque volutpat ac tincidunt vitae ' +
    'semper quis. Sit amet luctus venenatis lectus magna fringilla urna ' +
    'porttitor. Quis vel eros donec ac odio tempor orci dapibus. Morbi enim ' +
    'nunc faucibus a pellentesque sit amet. Fusce id velit ut tortor pretium ' +
    'viverra. Diam donec adipiscing tristique risus nec feugiat in fermentum ' +
    'posuere. Consectetur a erat nam at lectus urna duis convallis ' +
    'convallis. Hac habitasse platea dictumst vestibulum rhoncus est. Felis ' +
    'eget velit aliquet sagittis id consectetur purus. Quam lacus ' +
    'suspendisse faucibus interdum posuere lorem. Urna duis convallis ' +
    'convallis tellus. Sed risus pretium quam vulputate dignissim ' +
    'suspendisse in est ante. Consequat mauris nunc congue nisi vitae ' +
    'suscipit tellus mauris. Sit amet tellus cras adipiscing enim eu.\n\n' +
    'Enim nunc faucibus a pellentesque sit amet porttitor. Diam ut venenatis ' +
    'tellus in. Sed pulvinar proin gravida hendrerit. Fames ac turpis ' +
    'egestas sed tempus. Sed euismod nisi porta lorem. Lectus mauris ' +
    'ultrices eros in. Aliquet porttitor lacus luctus accumsan tortor ' +
    'posuere. Mauris a diam maecenas sed enim ut. Sed viverra tellus in hac ' +
    'habitasse platea. Blandit volutpat maecenas volutpat blandit aliquam ' +
    'etiam erat. Bibendum enim facilisis gravida neque convallis. Ultrices ' +
    'mi tempus imperdiet nulla malesuada pellentesque elit eget gravida.\n' +
    'Augue lacus viverra vitae congue eu consequat ac felis.\n\n' +
    'Feugiat in fermentum posuere urna nec tincidunt. Viverra orci sagittis ' +
    'eu volutpat odio. Sapien pellentesque habitant morbi tristique. Nunc ' +
    'pulvinar sapien et ligula ullamcorper malesuada proin libero nunc.\n' +
    'Ipsum consequat nisl vel pretium lectus quam. Sem nulla pharetra diam ' +
    'sit amet nisl suscipit. Sapien faucibus et molestie ac. Magnis dis ' +
    'parturient montes nascetur ridiculus mus mauris vitae. Et pharetra ' +
    'pharetra massa massa ultricies mi quis. Porttitor lacus luctus accumsan ' +
    'tortor posuere ac ut consequat. Gravida neque convallis a cras semper ' +
    'auctor neque vitae tempus.\n\n' +
    'Volutpat sed cras ornare arcu. Scelerisque varius morbi enim nunc ' +
    'faucibus a pellentesque sit. Velit scelerisque in dictum non ' +
    'consectetur a. Semper eget duis at tellus at urna condimentum. Massa ' +
    'vitae tortor condimentum lacinia quis vel eros. Et sollicitudin ac orci ' +
    'phasellus egestas. Mattis pellentesque id nibh tortor id aliquet.\n' +
    'Aliquet nec ullamcorper sit amet. Non enim praesent elementum facilisis ' +
    'leo vel fringilla. Et ultrices neque ornare aenean. Donec et odio ' +
    'pellentesque diam volutpat. Tincidunt augue interdum velit euismod in ' +
    'pellentesque massa. Vel elit scelerisque mauris pellentesque pulvinar.\n' +
    '\n' +
    'Amet nisl purus in mollis nunc sed. Quis risus sed vulputate odio ut ' +
    'enim blandit volutpat maecenas. Vel fringilla est ullamcorper eget ' +
    'nulla facilisi etiam dignissim. Elementum integer enim neque volutpat ' +
    'ac tincidunt vitae semper quis. Nisi lacus sed viverra tellus in hac ' +
    'habitasse platea dictumst. Elementum tempus egestas sed sed risus ' +
    'pretium. Viverra maecenas accumsan lacus vel facilisis volutpat est ' +
    'velit egestas. Sed felis eget velit aliquet sagittis id consectetur ' +
    'purus. Massa ultricies mi quis hendrerit dolor. Faucibus a pellentesque ' +
    'sit amet porttitor eget dolor morbi. Ut eu sem integer vitae. Dictum ' +
    'fusce ut placerat orci nulla. Vulputate enim nulla aliquet porttitor ' +
    'lacus luctus. Fames ac turpis egestas sed tempus. Venenatis a ' +
    'condimentum vitae sapien pellentesque habitant morbi tristique ' +
    'senectus. Sit amet commodo nulla facilisi nullam vehicula ipsum a arcu.' +
    '\n' +
    'Felis donec et odio pellentesque diam. Amet porttitor eget dolor morbi ' +
    'non arcu. Amet massa vitae tortor condimentum lacinia quis vel.\n\n' +
    'Urna condimentum mattis pellentesque id nibh tortor id aliquet lectus.\n' +
    'Amet purus gravida quis blandit turpis cursus in hac habitasse. Sed ' +
    'blandit libero volutpat sed cras. In eu mi bibendum neque egestas ' +
    'congue quisque. Ultricies leo integer malesuada nunc vel risus commodo ' +
    'viverra. At risus viverra adipiscing at in tellus. Leo vel orci porta ' +
    'non pulvinar neque. Nunc sed augue lacus viverra vitae congue. Donec ac ' +
    'odio tempor orci dapibus ultrices. Risus sed vulputate odio ut enim ' +
    'blandit volutpat maecenas. Faucibus et molestie ac feugiat sed lectus ' +
    'vestibulum. Arcu ac tortor dignissim convallis aenean et tortor. Est ' +
    'ante in nibh mauris cursus mattis molestie a iaculis. Donec massa ' +
    'sapien faucibus et. Velit dignissim sodales ut eu sem integer vitae.\n\n' +
    'Ac odio tempor orci dapibus. Non odio euismod lacinia at. Tellus ' +
    'elementum sagittis vitae et leo duis ut diam quam. Mattis rhoncus urna ' +
    'neque viverra justo nec ultrices dui. Leo a diam sollicitudin tempor ' +
    'id. Erat imperdiet sed euismod nisi porta lorem mollis aliquam ut.\n' +
    'Aliquam ultrices sagittis orci a scelerisque purus semper eget. Lacus ' +
    'vel facilisis volutpat est velit. Ornare massa eget egestas purus ' +
    'viverra accumsan in. Vitae purus faucibus ornare suspendisse. Aliquam ' +
    'faucibus purus in massa tempor nec feugiat nisl. Vulputate sapien nec ' +
    'sagittis aliquam malesuada bibendum arcu. Sit amet volutpat consequat ' +
    'mauris. Neque laoreet suspendisse interdum consectetur libero id. Est ' +
    'velit egestas dui id ornare arcu odio ut sem. Sed augue lacus viverra ' +
    'vitae. Scelerisque in dictum non consectetur a erat nam. Feugiat in ' +
    'ante metus dictum at.\n\n' +
    'Dui vivamus arcu felis bibendum ut tristique. Enim tortor at auctor ' +
    'urna. Sed augue lacus viverra vitae congue eu. Enim nulla aliquet ' +
    'porttitor lacus luctus accumsan tortor posuere ac. Faucibus vitae ' +
    'aliquet nec ullamcorper sit amet risus nullam. Cursus risus at ultrices ' +
    'mi tempus imperdiet nulla malesuada pellentesque. Amet luctus venenatis ' +
    'lectus magna fringilla urna porttitor rhoncus. Volutpat lacus laoreet ' +
    'non curabitur gravida arcu ac. Eget nullam non nisi est. Etiam ' +
    'dignissim diam quis enim lobortis scelerisque fermentum dui. Phasellus ' +
    'vestibulum lorem sed risus ultricies tristique nulla aliquet enim.\n' +
    'Sagittis nisl rhoncus mattis rhoncus urna neque viverra justo nec.\n' +
    'Elementum nisi quis eleifend quam adipiscing vitae.\n\n' +
    'Orci ac auctor augue mauris. Quis risus sed vulputate odio ut enim.\n' +
    'Vitae et leo duis ut diam quam nulla porttitor. Aliquet porttitor lacus ' +
    'luctus accumsan tortor posuere ac. Condimentum mattis pellentesque id ' +
    'nibh tortor. Odio morbi quis commodo odio aenean sed. Varius duis at ' +
    'consectetur lorem donec massa. Sagittis vitae et leo duis. Tellus ' +
    'elementum sagittis vitae et leo duis ut diam quam. Eget lorem dolor sed ' +
    'viverra ipsum nunc aliquet bibendum. Quis lectus nulla at volutpat diam ' +
    'ut. Auctor elit sed vulputate mi. Neque volutpat ac tincidunt vitae ' +
    'semper quis. Neque vitae tempus quam pellentesque nec nam.\n\n' +
    'Purus sit amet luctus venenatis lectus magna fringilla urna porttitor.\n' +
    'Odio pellentesque diam volutpat commodo sed egestas egestas fringilla ' +
    'phasellus. Gravida neque convallis a cras semper auctor. Nunc aliquet ' +
    'bibendum enim facilisis gravida neque convallis a. In aliquam sem ' +
    'fringilla ut morbi tincidunt. Sit amet est placerat in egestas erat ' +
    'imperdiet sed. Aliquam malesuada bibendum arcu vitae elementum. Nunc ' +
    'vel risus commodo viverra maecenas accumsan lacus vel. Auctor neque ' +
    'vitae tempus quam pellentesque nec. Eget lorem dolor sed viverra ipsum ' +
    'nunc aliquet bibendum enim. Odio euismod lacinia at quis risus sed ' +
    'vulputate odio ut. Ut ornare lectus sit amet est placerat in egestas.\n' +
    'Commodo ullamcorper a lacus vestibulum sed arcu non odio euismod.\n' +
    'Placerat duis ultricies lacus sed turpis tincidunt. Quis vel eros donec ' +
    'ac odio tempor orci. Scelerisque purus semper eget duis. Sapien nec ' +
    'sagittis aliquam malesuada bibendum arcu vitae elementum curabitur.\n' +
    'Cras fermentum odio eu feugiat pretium nibh ipsum consequat nisl.\n\n' +
    'Hendrerit dolor magna eget est lorem ipsum dolor sit amet. Dictumst ' +
    'vestibulum rhoncus est pellentesque elit ullamcorper. Ut consequat ' +
    'semper viverra nam libero. Ipsum dolor sit amet consectetur adipiscing.' +
    '\n' +
    'Tristique risus nec feugiat in fermentum posuere urna nec tincidunt.\n' +
    'Sit amet mauris commodo quis imperdiet massa. Varius morbi enim nunc ' +
    'faucibus. Adipiscing diam donec adipiscing tristique risus nec feugiat ' +
    'in fermentum. Consequat id porta nibh venenatis cras sed felis eget.\n' +
    'Tellus molestie nunc non blandit massa enim nec dui. Odio morbi quis ' +
    'commodo odio aenean sed adipiscing diam donec. Diam donec adipiscing ' +
    'tristique risus nec. Scelerisque eu ultrices vitae auctor eu augue ut ' +
    'lectus. Tellus pellentesque eu tincidunt tortor aliquam. Fermentum leo ' +
    'vel orci porta non pulvinar neque laoreet suspendisse.\n';

export const fakeLogSavePath = 'fake/save/path';
