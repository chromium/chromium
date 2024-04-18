// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_MAHI_MAHI_CONTENT_EXTRACTION_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_MAHI_MAHI_CONTENT_EXTRACTION_DELEGATE_H_

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/unguessable_token.h"
#include "chromeos/components/mahi/public/mojom/content_extraction.mojom.h"
#include "chromeos/crosapi/mojom/mahi.mojom-forward.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "url/gurl.h"

namespace mahi {

struct WebContentState;

using GetContentCallback =
    base::OnceCallback<void(crosapi::mojom::MahiPageContentPtr)>;

// This is the delegate of the mahi content extraction service. It is
// responsible for the service setup, binding and requests.
class MahiContentExtractionDelegate {
 public:
  explicit MahiContentExtractionDelegate(
      base::RepeatingCallback<void(const base::UnguessableToken&, bool)>
          distillable_check_callback);
  MahiContentExtractionDelegate(const MahiContentExtractionDelegate&) = delete;
  MahiContentExtractionDelegate& operator=(
      const MahiContentExtractionDelegate&) = delete;
  ~MahiContentExtractionDelegate();

  // Returns true if it requires a new content extraction service, and false
  // otherwise.
  bool SetUpContentExtractionService();

  void EnsureServiceIsConnected();

  // Requests the content extraction service to get content from the a11y
  // update. Returns nullptr if the content cannot be extracted.
  void ExtractContent(const WebContentState& web_content_state,
                      const base::UnguessableToken& client_id,
                      GetContentCallback callback);

  // Requests the content extraction service to check the page distillability
  // based on the a11y update. `distillable_check_callback_` will be triggered
  // when the check is finished.
  void CheckDistillablity(const WebContentState& web_content_state,
                          const base::Time& start_time);

 private:
  void OnGetContentSize(const base::UnguessableToken& page_id,
                        const base::Time& start_time,
                        mojom::ContentSizeResponsePtr response);

  void OnGetContent(const base::UnguessableToken& page_id,
                    const base::UnguessableToken& client_id,
                    const GURL& url,
                    GetContentCallback callback,
                    mojom::ExtractionResponsePtr response);

  // Callback when screen ai service is initialized. If successful, binds mahi
  // content extraction service with the screen ai main content extraction
  // model.
  void OnScreenAIServiceInitialized(bool successful);

  mojo::Remote<mojom::ContentExtractionServiceFactory>
      remote_content_extraction_service_factory_;
  mojo::Remote<mojom::ContentExtractionService>
      remote_content_extraction_service_;

  // This is the callback function to notifies the `MahiWebContentManager` with
  // the distillability check result.
  base::RepeatingCallback<void(const base::UnguessableToken&, bool)>
      distillable_check_callback_;

  // This task runner is used to save the extracted content to disk. It meant to
  // be used for debugging purposes only, and should not be used in production.
  const scoped_refptr<base::SequencedTaskRunner> io_task_runner_;

  base::WeakPtrFactory<MahiContentExtractionDelegate> weak_pointer_factory_{
      this};
};

}  // namespace mahi

#endif  // CHROME_BROWSER_CHROMEOS_MAHI_MAHI_CONTENT_EXTRACTION_DELEGATE_H_
