// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_INSTANCE_IMPL_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_INSTANCE_IMPL_H_

#include <string>

#include "chrome/browser/ash/input_method/mojom/editor.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace input_method {

class EditorInstanceImpl : public mojom::EditorInstance {
 public:
  EditorInstanceImpl();
  ~EditorInstanceImpl() override;

  // mojom::EditorInstance overrides
  void GetRewritePresetTextQueries(
      GetRewritePresetTextQueriesCallback callback) override;
  void CommitEditorResult(const std::string& text,
                          CommitEditorResultCallback callback) override;

  // Binds a new receiver to this instance. The instance maintains a set of
  // receivers and can service multiple connections at one time (ie. two ui
  // clients simultaneously).
  void BindReceiver(
      mojo::PendingReceiver<mojom::EditorInstance> pending_receiver);

 private:
  // Holds any connections from the ui to an EditorInstance. A set of receivers
  // is maintained to ensure we can handle multiple connections.
  mojo::ReceiverSet<mojom::EditorInstance> receivers_;
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_EDITOR_INSTANCE_IMPL_H_
