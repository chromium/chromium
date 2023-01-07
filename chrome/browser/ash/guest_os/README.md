# Guest OS

This directory contains code to interact with Chrome OS guest
[VMs and containers](https://chromium.googlesource.com/chromiumos/docs/+/main/containers_and_vms.md)
This directory includes code which is common to all VM types such as file
sharing.

Code for specific VM types can be found in:
* Crostini [`chrome/browser/ash/crostini`](/chrome/browser/ash/crostini/)
* PluginVm [`chrome/browser/ash/plugin_vm`](/chrome/browser/ash/plugin_vm/)
