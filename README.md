
This repository is a chromium fork that has been adapted to use the record/replay driver.

# Setting up builds

Only one build configuration is currently supported.

1. Install chromium tree and `depot_tools` per https://www.chromium.org/developers/how-tos/get-the-code
   * Warning: The `--no-history` flag is currently untested. Try it if you don't think you won't need to deal with rebase in the future.
2. Change to and pull our (instead of the original) origin for chromium.
   * NOTE: The `gclient sync` here updates all subrepositories to the correct point, including both repositories we've modified and ones we haven't.  See "Setting dependency revisions" below for more.
   * NOTE: After `git pull`, you might see "You are not currently on a branch. Please specify which branch you want to merge with.". In this case, `git switch master` will ignore all local changes, and instead tracks and switches to the remote `master` on the current branch ([more info here](https://stackoverflow.com/a/9537923)).
   ```sh
   cd /path/to/chromium/src
   git remote set-url origin https://github.com/replayio/chromium.git
   git branch -D main
   git pull
   git switch master
   gclient sync
   ```
3. Setup engflow:
   ```sh
   export GOMA_SERVER_HOST=simpsonite.goma.engflow.com
   export GOMACTL_USE_PROXY=false
   goma_auth login
   goma_ctl restart
   ```
4. Gen + Configure your build:
   * If you are using Linux, see [Troubleshooting](#troubleshooting) first.
   ```
   cd /path/to/src
   gn gen out/Release
   gn args out/Release # opens args.gn config file
   ```
5. Add the following settings to `out/Release/args.gn`:
   ```ini
   # src/out/Release/args.gn

   use_goma = true # if using engflow, false otherwise
   is_debug = false # we can't really use most debugging symbols right now
   dcheck_always_on = false # disable dchecks
   enable_nacl = false  # no native targets
   ```
   * On linux add the following settings to disable the allocator shim.
     ```ini
     use_allocator = "none"
     use_allocator_shim = false
     ```
     * NOTE: This may not be necessary but avoids interactions between the driver and chromium.
   * On linux the following setting may be helpful for looking at stack traces while recording.
     ```ini
     symbol_level = 1
     ```
     * NOTE: This isn't used for production builds because the resulting binary size is pretty huge.
6. Build:
   ```
   node build
   ```

# Troubleshooting

If you have trouble with new submodules popping up that are not part of our current release:

* You might see the submodules pop up as unwanted files in `git status`, and they might sneak into your PR.
* You can easily and safely remove them via `git rm --cached name-of-submodule`.
* E.g. (for old Chromium 91): `git rm --cached docs/website third_party/cast_core/public/src third_party/content_analysis_sdk/src third_party/cpuinfo/src third_party/cros_components third_party/fxdiv/src third_party/highway/src third_party/libjxl/src third_party/pthreadpool/src third_party/wayland-protocols/gtk third_party/wayland-protocols/kde third_party/xnnpack/src`


## Missing symlinks

When facing the following:
```
$ gn gen out/Release
ERROR at //build/toolchain/concurrent_links.gni:90:19: Script returned non-zero exit code.
  _command_dict = exec_script("get_concurrent_links.py", _args, "scope")
                  ^----------
```

Try to install some more additional Python-related packages:
```
sudo apt install python-pkg-resources python3-pkg-resources python-is-python3
```

## libssl on Ubuntu 22.04

If launching `out/release/chrome` gives this error message:
```
Loading Record Replay driver failed.
```

This is caused by [missing libssl 1.1 on Ubuntu 22.04](https://askubuntu.com/questions/1424442/libssl1-1-is-deprecated-in-ubuntu-22-04-what-to-do-now), apparently a common problem that breaks many popular software (MongoDB, MariaDB, etc.)

*Solution*: Either stick with Ubuntu 20.04, or (as a workaround) install `libssl 1.1` manually from Ubuntu 20.04:

```sh
echo "deb http://security.ubuntu.com/ubuntu focal-updates main" | sudo tee /etc/apt/sources.list.d/focal-updates.list
sudo apt update
sudo apt install libssl1.1
```

# Rebase: Merging from upstream

Because chromium's source is split across many git repositories, merging changes from upstream is tricky

Pull upstream changes and merge into master branch:

```sh
git checkout upstream
git pull https://github.com/chromium/chromium.git master
git push
git checkout master
git merge upstream
... fix merge conflicts ...
git commit -a
git push
```

## Update other dependencies

```sh
cd /path/to/chromium
gclient sync -D
```

### V8

```sh
cd v8
git pull
... fix merge conflicts ...
git commit -a
git push
```

-> Repeat for all other chromium repositories we've forked (`skia`, `webrtc` etc.)


## Setting dependency revisions

The revision to use for dependent repositories is specified in the [DEPS](./DEPS) file and updated to by running `gclient sync`.  Whenever the revision to use for any dependencies we've modified changes, this file needs to be updated.  Look for `v8_revision`, `skia_revision`, or the revision associated with `https://github.com/replayio/chromium-webrtc.git`.


## Adopting a rebased version

After a rebase has happened (e.g. `master` has been rebased to latest `chromium` release version):

1. `git checkout master`
2. `git pull`
3. Update submodules
   ```sh
   cd ./v8 && git checkout master && git pull && \
   cd ../third_party/webrtc && git checkout main && git pull && \
   cd ../skia && git checkout main && git pull && \
   cd ../..
   ```
4. (To play it extra safe) Verify that submodule revisions are correct in `DEPS`, e.g. via:
   ```sh
   cd ./v8 && git log
   cd ../third_party/webrtc && git log
   cd ../skia && git log
   cd ../..
   ```
5. Update your local `depot_tools`
6. If possible, update build dependencies: `./build/install-build-deps.sh` (might not always work because it does not support many systems)
7. Clean your build (delete `out` folder)
8. Re-do the initial steps:
   ```sh
   cd .../src
   gclient sync -D

   # NOTE: `args.gn` has been deleted - fix it again (see above for details)
   gn gen out/Release
   gn args out/Release # or manually write `./out/Release/args.gn`

   # WARN: If you did any of the above steps wrong, nuke `out` and try again.
   node build
   ```
