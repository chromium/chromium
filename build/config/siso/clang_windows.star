# -*- bazel-starlark -*-
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Siso configuration for clang-cl/windows."""

load("@builtin//path.star", "path")
load("@builtin//struct.star", "module")
load("./clang_all.star", "clang_all")
load("./clang_code_coverage_wrapper.star", "clang_code_coverage_wrapper")
load("./rewrapper_cfg.star", "rewrapper_cfg")

__filegroups = {
    # for precomputed subtree
    "third_party/depot_tools/win_toolchain/vs_files/27370823e7:headers-ci": {
        "type": "glob",
        "includes": [
            "*.h",
            "*.inl",
            "*.H",
            "*.Hxx",
            "*.hxx",
            "*.hpp",
            "VC/Tools/MSVC/*/include/*",
            "VC/Tools/MSVC/*/include/*/*",
        ],
    },
}

__filegroups.update(clang_all.filegroups)

def __clang_compile_coverage(ctx, cmd):
    clang_command = clang_code_coverage_wrapper.run(ctx, list(cmd.args))
    ctx.actions.fix(args = clang_command)

__handlers = {
    "clang_compile_coverage": __clang_compile_coverage,
}

def __step_config(ctx, step_config):
    cfg = "buildtools/reclient_cfgs/chromium-browser-clang/rewrapper_windows.cfg"
    if ctx.fs.exists(cfg):
        reproxy_config = rewrapper_cfg.parse(ctx, cfg)
        step_config["platforms"].update({
            "clang-cl": reproxy_config["platform"],
        })
        step_config["input_deps"].update(clang_all.input_deps)
        if reproxy_config["platform"]["OSFamily"] == "Windows":
            step_config["input_deps"].update({
                "third_party/depot_tools/win_toolchain/vs_files/27370823e7:headers": [
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7:headers-ci",
                ],
            })
        else:
            step_config["input_deps"].update({
                "third_party/depot_tools/win_toolchain/vs_files/27370823e7:headers": [
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7:headers-ci",
                    # third_party/libc++ includes "DeplayIMP.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/VC/Tools/MSVC/14.34.31933/include/DelayIMP.h",
                    # third_party/abseil-cpp includes "dbghelp.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/dbghelp.h",
                    # third_party/abseil-cpp includes "aclapi.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/aclapi.h",
                    # base/debug includes "psapi.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/psapi.h",
                    # base/process includes "tlhelp32.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/tlhelp32.h",
                    # base/process includes "userenv.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/userenv.h",
                    # base includes "shlobj.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/shlobj.h",
                    # base/win includes "lm.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/lm.h",
                    # base/win includes "mdmregistration.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/mdmregistration.h",
                    # base/win includes "shellscalingapi.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/shellscalingapi.h",
                    # base/win includes "uiviewsettingsinterop.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/uiviewsettingsinterop.h",
                    # native_client/src/shared/platform/win includes "WinError.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/shared/WinError.h",
                    # third_party/webrtc/rtc_base/win includes "windows.graphics.directX.direct3d11.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/cppwinrt/winrt/windows.graphics.directX.direct3d11.h",
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/winrt/windows.graphics.directX.direct3d11.h",
                    # third_party/webrtc/rtc_base/win includes "windows.graphics.directX.direct3d11.interop.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/windows.graphics.directX.direct3d11.interop.h",
                    # third_party/crashpad/crashpad/handler/win includes "werapi.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/werapi.h",
                    # chrome/install_static/ includes "wtsapi32.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/wtsapi32.h",
                    # third_party/dawn/include/dawn/native includes "DXGI1_4.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/shared/DXGI1_4.h",
                    # v8/src/diagnostics includes "versionhelpers.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/versionhelpers.h",
                    # ui/gfx/ includes "DXGIType.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/shared/DXGIType.h",
                    # third_party/unrar includes "PowrProf.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/PowrProf.h",
                    # device/base/ includes "dbt.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/dbt.h",
                    # third_party/skia/ includes "ObjBase.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/ObjBase.h",
                    # third_party/webrtc/rtc_base includes "ws2spi.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/ws2spi.h",
                    # third_party/skia/ includes "T2EmbApi.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/T2EmbApi.h",
                    # device/vr/windows/ includes "D3D11_1.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/D3D11_1.h",
                    # rlz/win/ includes "Sddl.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/shared/Sddl.h",
                    # chrome/common/safe_browsing/ includes "softpub.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/softpub.h",
                    # services/device/generic_sensor/ includes "Sensors.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/Sensors.h",
                    # third_party/webrtc/modules/desktop_capture/win includes "windows.graphics.capture.interop.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/windows.graphics.capture.interop.h",
                    # third_party/skia/ includes "FontSub.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/FontSub.h",
                    # chrome/updater/ includes "regstr.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/regstr.h",
                    # services/device/compute_pressure includes "pdh.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/pdh.h",
                    # chrome/installer/ includes "mshtmhst.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/mshtmhst.h",
                    # net/ssl/ includes "NCrypt.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/NCrypt.h",
                    # device/fido/win/ includes "Combaseapi.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/Combaseapi.h",
                    # components/device_signals/core/system_signals/win includes "wscapi.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/wscapi.h",
                    # net/proxy_resolution/win/ includes "dhcpcsdk.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/dhcpcsdk.h",
                    # third_party/dawn/third_party/glfw includes "xinput.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/xinput.h",
                    # v8/tools/v8windbg includes "pathcch.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/pathcch.h",
                    # remoting/host includes "rpcproxy.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/rpcproxy.h",
                    # sandbox/win includes "Aclapi.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/Aclapi.h",
                    # ui/accessibility/platform includes "uiautomation.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/uiautomation.h",
                    # chrome/credential_provider/gaiacp includes "ntsecapi.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/ntsecapi.h",
                    # net/dns includes "Winsock2.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/Winsock2.h",
                    # media/cdm/win includes "mferror.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/mferror.h",
                    # chrome/credentialProvider/gaiacp includes "Winternl.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/Winternl.h",
                    # media/audio/win includes "audioclient.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/audioclient.h",
                    # media/audio/win includes "MMDeviceAPI.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/MMDeviceAPI.h",
                    # net/proxy_resolution/win includes "dhcpv6csdk.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/dhcpv6csdk.h",
                    # components/system_media_controls/win includes "systemmediatransportcontrolsinterop.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/systemmediatransportcontrolsinterop.h",
                    # ui/native_theme includes "Windows.Media.ClosedCaptioning.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/cppwinrt/winrt/Windows.Media.ClosedCaptioning.h",
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/winrt/Windows.Media.ClosedCaptioning.h",
                    # media/audio/win includes "Functiondiscoverykeys_devpkey.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/Functiondiscoverykeys_devpkey.h",
                    # device/fido includes "Winuser.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/Winuser.h",
                    # chrome/updater/win includes "msxml2.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/msxml2.h",
                    # remoting/host includes "ime.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/ime.h",
                    # remoting/host/win includes "D3DCommon.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/D3DCommon.h",
                    # ui/views/controls/menu includes "Vssym32.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/Vssym32.h",
                    # third_party/wtl includes "richedit.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/richedit.h",
                    # chrome/updater/net includes "Urlmon.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/Urlmon.h",
                    # device/gamepad includes "XInput.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/XInput.h",
                    # chrome/credential_provider/gaiacp includes "Shlobj.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/Shlobj.h",
                    # content/renderer includes "mlang.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/mlang.h",
                    # components/storage_monitor includes "portabledevice.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/portabledevice.h",
                    # third_party/wtl includes "richole.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/richole.h",
                    # chrome/utility/importer includes "intshcut.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/intshcut.h",
                    # chrome/browser/net includes "Ws2spi.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/Ws2spi.h",
                    # chrome/browser/enterprise/platform_auth includes "proofofpossessioncookieinfo.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/proofofpossessioncookieinfo.h",
                    # chrome/utility/importer includes "urlhist.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/urlhist.h",
                    # chrome/updater/win/installer includes "msiquery.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/um/msiquery.h",
                    # third_party/win_virtual_display/controller includes "Devpropdef.h"
                    "third_party/depot_tools/win_toolchain/vs_files/27370823e7/Windows Kits/10/Include/10.0.22621.0/shared/Devpropdef.h",
                ],
            })
        step_config["rules"].extend([
            {
                "name": "clang-cl/cxx",
                "action": "(.*_)?cxx",
                "command_prefix": "..\\..\\third_party\\llvm-build\\Release+Asserts\\bin\\clang-cl.exe",
                "platform_ref": "clang-cl",
                "remote": True,
                "remote_wrapper": reproxy_config["remote_wrapper"],
                "timeout": "2m",
            },
            {
                "name": "clang-cl/cc",
                "action": "(.*_)?cc",
                "command_prefix": "..\\..\\third_party\\llvm-build\\Release+Asserts\\bin\\clang-cl.exe",
                "platform_ref": "clang-cl",
                "remote": True,
                "remote_wrapper": reproxy_config["remote_wrapper"],
                "timeout": "2m",
            },
            {
                "name": "clang-coverage/cxx",
                "action": "(.*_)?cxx",
                "command_prefix": "python3.exe ../../build/toolchain/clang_code_coverage_wrapper.py",
                "inputs": [
                    "third_party/llvm-build/Release+Asserts/bin/clang++",
                ],
                "handler": "clang_compile_coverage",
                "platform_ref": "clang-cl",
                "remote": True,
                "remote_wrapper": reproxy_config["remote_wrapper"],
                "timeout": "2m",
            },
            {
                "name": "clang-coverage/cc",
                "action": "(.*_)?cc",
                "command_prefix": "python3.exe ../../build/toolchain/clang_code_coverage_wrapper.py",
                "inputs": [
                    "third_party/llvm-build/Release+Asserts/bin/clang",
                ],
                "handler": "clang_compile_coverage",
                "platform_ref": "clang-cl",
                "remote": True,
                "remote_wrapper": reproxy_config["remote_wrapper"],
                "timeout": "2m",
            },
        ])
    return step_config

clang = module(
    "clang",
    step_config = __step_config,
    filegroups = __filegroups,
    handlers = __handlers,
)
