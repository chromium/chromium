import asyncio
from quart import Quart, request, jsonify
from transformers import pipeline

app = Quart(__name__)

# Global variable to hold the QA pipeline.
qa_pipeline = None

@app.route('/load_model_bert', methods=['POST'])
async def load_model_bert():
    global qa_pipeline
    try:
        # Load the MobileBERT QA pipeline using the PyTorch backend.
        qa_pipeline = pipeline(
            "question-answering",
            model="csarron/mobilebert-uncased-squad-v2",
            tokenizer="csarron/mobilebert-uncased-squad-v2",
            framework="pt"
        )
        return jsonify({"status": "Model loaded successfully."})
    except Exception as e:
        return jsonify({"error": f"Failed to load model: {str(e)}"}), 500

@app.route('/infer_single_bert', methods=['POST'])
async def infer_single_bert():
    global qa_pipeline
    if qa_pipeline is None:
        return jsonify({"error": "Model not loaded. Please call /load_model_bert first."}), 400

    data = await request.get_json()
    if not data or 'question' not in data or 'context' not in data:
        return jsonify({"error": "Payload must contain 'question' and 'context' fields."}), 400

    try:
        # Use asyncio.to_thread to run the inference in a separate thread
        prediction = await asyncio.to_thread(qa_pipeline, {"question": data['question'], "context": data['context']})
        return jsonify(prediction)
    except Exception as e:
        return jsonify({"error": f"Inference error: {str(e)}"}), 500

@app.route('/infer_batch_bert', methods=['POST'])
async def infer_batch_bert():
    global qa_pipeline
    if qa_pipeline is None:
        return jsonify({"error": "Model not loaded. Please call /load_model_bert first."}), 400

    data = await request.get_json()
    if not data or not isinstance(data, list):
        return jsonify({"error": "Payload must be an array of objects with 'question' and 'context' fields."}), 400

    results = []
    for idx, item in enumerate(data):
        if 'question' not in item or 'context' not in item:
            results.append({"error": f"Item at index {idx} is missing 'question' or 'context'."})
        else:
            try:
                pred = await asyncio.to_thread(qa_pipeline, {"question": item['question'], "context": item['context']})
                results.append(pred)
            except Exception as e:
                results.append({"error": f"Inference error at index {idx}: {str(e)}"})
    return jsonify(results)

if __name__ == '__main__':
    # For development, you can run using Quart's built-in server (not production-grade).
    # However, for performance, run this with Hypercorn with uvloop.
    app.run(host='0.0.0.0', port=5000, debug=True)

