# CfM GN Build Flags

Note: GN Flags are Build time flags

You can get a comprehensive list of all arguments supported by gn by running the
command gn args --list out/some-directory (the directory passed to gn args is
required as gn args will invokes gn gen to generate the build.ninja files).

## is_cfm (BUILDFLAG(PLATFORM_CFM))

Flag for building chromium for CfM devices.

### Query Flag
```bash
$ gn args out_<cfm_overlay>/{Release||Debug} --list=is_cfm
```

### Enable Flag
```bash
$ gn args out_<cfm_overlay>/{Release||Debug}
$ Editor will open add is_cfm=true save and exit
```

### Alt: EnrollmentRequisitionManager

We can alternatively use the EnrollmentRequisitionManager to determine if
chromium is running a CfM enabled Platform in source code

```cpp
policy::EnrollmentRequisitionManager::IsRemoraRequisition();
```
