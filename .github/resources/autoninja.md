## Identify Build command
Based on the `${out_dir}` and `${build_target}` determine the exact command to
build the test using the syntax `autoninja out -C out/{out_dir} {build_target}`.
- For example: if the `${out_dir}` is `debug_x64`, the command will be:
  `autoninja -k 0 -C out/debug_x64 build_target`

## Build and fix compile errors

You **must** run the build command to build the code after making the changes.
- If you encounter **any** compile errors, fix them before continuing and build
  again. This may involve adding or removing includes, changing method
  signatures, or adjusting the test logic to match the new browser test
  framework.
- It is expected that you will encounter some compile errors, so be prepared to
  fix them iteratively and build again if if necessary.

### Example Build Errors
If you encounter any build errors you could not fix in one try, that it would
have been helpful to have generic examples for, let the user know that
they can update this prompt to include this information in the future for faster
fixes.
