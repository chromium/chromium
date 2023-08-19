The project_simon resources cannot be made public until after the feature has
been launched. Thus, they are currently hosted in CIPD and downloaded to this
directory only if an internal chrome-branded checkout is being used. To update
the assets in cipd:
* `cd` to this directory in your local checkout
* Update the project_simon_strings/ directory locally with desired changes.
* Rebuild and test it.
* `cipd auth-login`
* `cipd create -pkg-def=cipd_project_simon_strings.yaml`.
  * That outputs something like this:

Instance: chromeos_internal/ash/peripherals-and-serviceability/shimless_rma_project_simon_strings:<version_id>
 • Instance chromeos_internal/ash/peripherals-and-serviceability/shimless_rma_project_simon_strings:<version_id> was successfully registered

* Open chromium/src/DEPS and find "src/ash/webui/shimless_rma/resources".
  Update the "version" field to the <version_id> printed above.