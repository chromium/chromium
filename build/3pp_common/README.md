# 3pp_common

Contains helper scripts for chromium 3pp configs.

## Usage

Most scripts assume the following `3pp.pb` format, which does as little as
possible in the recipes so that scripts can be tested locally.

```
create {
  source {
    script {
      name: "3pp.py"
      use_fetch_checkout_workflow: true
    }
  }

  build {
    install: ["3pp.py", "install"]
    # Any 3pp packages here are added to PATH by recipes.
    # For local testing, they must already exist on your PATH.
    tool: "chromium/third_party/maven"
  }
}

upload {
  # Assuming 3pp.pb is in //third_party/foo/bar/3pp/3pp.pb
  pkg_prefix: "chromium/third_party/foo"
  universal: true
}
```

Flow for local testing:

```
# Install any tools needed (that are listed as "tools" in 3pp.pb")
$ sudo apt-get install maven

# Tests all three commands.
$ 3pp/3pp.py local-test
```

To test individual steps:
```
$ 3pp/3pp.py latest
someversion.somehash

$ 3pp/3pp.py checkout /tmp/foo --version someversion.somehash

$ 3pp/3pp.py install out unused_dep_dir --version someversion.somehash --checkout-dir /tmp/foo
```

## References

* [`//docs/docs/cipd_and_3pp.md`](/docs/cipd_and_3pp.md)
* [`//build/recipes/recipe_modules/chromium_3pp/api.py`]https://source.chromium.org/chromium/infra/infra_superproject/+/main:build/recipes/recipe_modules/chromium_3pp/api.py)
