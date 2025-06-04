import asyncio
import time
import numpy as np
from quart import Quart, request, jsonify
import onnxruntime as ort
import uvloop

app = Quart(__name__)
session = None  # Global variable to hold the ONNX Runtime session

@app.route('/load_model_bert', methods=['POST'])
async def load_model_bert():
    global session
    try:
        # Load the exported ONNX MobileBERT model.
        session = ort.InferenceSession("./mobilebert.onnx")
        return jsonify({"status": "Model loaded successfully."})
    except Exception as e:
        return jsonify({"error": f"Failed to load model: {str(e)}"}), 500

@app.route('/infer_single_bert', methods=['POST'])
async def infer_single_bert():
    global session
    if session is None:
        return jsonify({"error": "Model not loaded. Please call /load_model_bert first."}), 400

    data = await request.get_json()
    if not data or ('question' not in data or 'context' not in data):
        return jsonify({"error": "Payload must contain 'question' and 'context' fields."}), 400

    # Using fixed dummy tokenized input for demonstration.
    # (In a production system, you would perform real tokenization.)
    input_ids_array = np.array([[101, 7592, 1010, 2026, 3899, 2003, 1010, 102, 0, 0]], dtype=np.int64)
    attention_mask_array = np.array([[1, 1, 1, 1, 1, 1, 1, 1, 0, 0]], dtype=np.int64)
    feeds = {
        'input_ids': input_ids_array,
        'attention_mask': attention_mask_array
    }
    try:
        output = session.run(None, feeds)
        # Assume the ONNX model returns an output with the key "logits".
        return jsonify({"logits": output["logits"].tolist()})
    except Exception as e:
        return jsonify({"error": f"Inference error: {str(e)}"}), 500

@app.route('/infer_batch_bert', methods=['POST'])
async def infer_batch_bert():
    global session
    if session is None:
        return jsonify({"error": "Model not loaded. Please call /load_model_bert first."}), 400

    data = await request.get_json()
    if not data or not isinstance(data, list):
        return jsonify({"error": "Payload must be an array."}), 400

    results = []
    # For each item in the batch, use the same dummy input as above.
    for i in range(len(data)):
        try:
            input_ids_array = np.array([[101, 7592, 1010, 2026, 3899, 2003, 1010, 102, 0, 0]], dtype=np.int64)
            attention_mask_array = np.array([[1, 1, 1, 1, 1, 1, 1, 1, 0, 0]], dtype=np.int64)
            feeds = {
                'input_ids': input_ids_array,
                'attention_mask': attention_mask_array
            }
            output = session.run(None, feeds)
            results.append({"logits": output["logits"].tolist()})
        except Exception as e:
            results.append({"error": f"Inference error at index {i}: {str(e)}"})
    return jsonify(results)

@app.route('/trainModel', methods=['POST'])
async def train_model():
    # Simulate training by sleeping for 2 seconds.
    start = time.perf_counter()
    await asyncio.sleep(2)
    end = time.perf_counter()
    training_time_ms = (end - start) * 1000
    return jsonify({"training_time_ms": training_time_ms, "accuracy": 0.75})

if __name__ == '__main__':
    # Install uvloop for improved performance.
    uvloop.install()
    # For local testing, you can use Quart's built-in server.
    # For production, run with hypercorn, e.g.:
    #   hypercorn server:app --bind 0.0.0.0:5000 --worker-class uvloop --log-level debug
    app.run(host='0.0.0.0', port=5000, debug=True)

