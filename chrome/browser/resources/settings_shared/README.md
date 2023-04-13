This directory contains code that is shared between

 * Browser Settings (chrome://settings)
 * CrOS Settings (chrome://os-settings)

The pattern used here is referred to as "limited sharing" unlike the
chrome://resources approach which is "unlimited sharing".

This approach has the following advantages over moving shared code under
chrome://resources/:

 * It allows a limited number of WebUI surfaces to use it, instead of exposing
   to all WebUI surfaces (hence the "limited sharing" name).
 * It also allows leveraging $i18n{} C++ string replacements, which is not
   possible in cr_elements/ or cr_components/. WebUIs that use the shared code
   are still responsible for registering all necessary strings.

The "limited sharing" mechanism works as follows

 * Code to be shared is built on its own, with a dedicated
   build_webui() target.
 * The resulting grd file entries `kSettingsSharedResources` are included in the
   build once and registered at runtime under all WebUIDataSource instances that
   need to use this code.
 * Shared code is exposed under /shared/settings/* URLs (same as
   chrome://settings/shared/settings/ or chrome://os-settings/shared/settings/
   respectively).
 * build_webui()'s `ts_path_mappings` and `optimize_webui_external_paths`
   parameters are leveraged so that the respective underlying tools (TS compiler
   and Rollup) can correctly resolve such paths.

A similar approach is also followed in c/b/r/side_panel/shared.

Note: The files in this folder are only added as standalone files to the build
when `optimize_webui=false`. In other cases the same files are already bundled
within the UIs that use them and therefore adding them as standalone files is
unnecessary.
