# Ash Crosapi Test

## Overview

This is a black box testing suite to verify the crosapi behavior, named Crosapi Test.
Crosapi Test can be used to check if Ash returns the expected value when a crosapi function is
called.

The key points that make Crosapi Test different from other tests are:
* This test suite focuses on Ash and runs without Lacros.
* This test suite checks the crosapi behavior from outside of Ash.

See [Ash Crosapi Testing Framework](https://docs.google.com/document/d/1S-dTDiEI-oE8L8BvgoIe4hD_yDNBrULEjA8V2QSbKiA/edit?usp=sharing&resourcekey=0-q0gxab28Z-mQqE_QtdYvfg) for more details.


## How To Write

You can write tests by creating a class that inherits [CrosapiTestBase](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/crosapi/test/crosapi_test_base.h), and the new class has remote instance(s). You can use individual interfaces by calling [CrosapiTestBase::BindCrosapiInterface()](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/crosapi/test/crosapi_test_base.h) with bind functions (e.g. [BindNetworkChange](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/crosapi/mojom/crosapi.mojom?q=BindNetworkChange%20f:crosapi.mojom) or [BindFileManager](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/crosapi/mojom/crosapi.mojom?q=BindFileManager%20f:crosapi.mojom)).

This is an example code for [FileManager](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/crosapi/mojom/file_manager.mojom).

```
// Inherit CrosapiTestBase to use the test suite.
class FileManagerCrosapiTest : public CrosapiTestBase {
 protected:
  void SetUp() override {
    // You need to call the parent's SetUp() to check crosapi connection is set up.
    CrosapiTestBase::SetUp();

    // Pass bind function, then you can use crosapi function.
    file_manager_ = BindCrosapiInterface(&mojom::Crosapi::BindFileManger);
  }

  mojo::Remote<mojom::FileManager> file_manager_;
};

TEST_F(FileManagerCrosapiTest, ShowItemInFolder) {
  base::test::TestFuture<mojom::OpenResult> future;

  file_manager_->ShowItemInFolder(kNonExistingPath, future.GetCallback());
  EXPECT_EQ(future.Get(), mojom::OpenResult::kFailedPathNotFound);
}
```

**Note :** You cannot modify Ash states directly because the test process is separated from Ash process. Instead, you
can use [`TestController`](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/crosapi/mojom/test_controller.mojom) to manipulate Ash if the test target is `test_ash_chrome`.

### Example tests
* [Networkchange crosapi test](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/crosapi/test/network_change_ash_crosapitest.cc)
