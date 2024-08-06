# ![Logo](chrome/app/theme/chromium/product_logo_64.png) Chrobalt Sandbox

This is Cobalt's repo for testing changes as we move to Chrobalt.

To check out the source code locally, don't use `git clone`! Instead,
follow [the instructions on how to get the code](docs/get_the_code.md).
Then add chrobalt_sandbox as a remote and fetch it:

```shell
cd src
git remote add chrobalt_sandbox git@github.com:youtube/chrobalt_sandbox.git
git fetch chrobalt_sandbox
git checkout -b dev/m114 chrobalt_sandbox/dev/m114
gclient sync -r $(git rev-parse HEAD)
```

Then to build linux:

```shell
gn gen out/Default
cp chrobalt/linux/args.gn out/Default/
gn gen out/Default
ninja -C out/Default base_unittests
```

Replacing `base_unittests` with whatever target you want to build.
